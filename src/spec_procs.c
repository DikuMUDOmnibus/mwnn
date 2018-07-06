/* ************************************************************************
*   File: spec_procs.c                                  Part of CircleMUD *
*  Usage: implementation of special procedures for mobiles/objects/rooms  *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "spells.h"
#include "constants.h"


/*   external vars  */
extern struct room_data *world;
extern struct char_data *character_list;
extern struct descriptor_data *descriptor_list;
extern struct index_data *mob_index;
extern struct index_data *obj_index;
extern struct time_info_data time_info;
extern struct spell_info_type spell_info[];
extern int guild_info[][3];
extern int train_params[6][NUM_CLASSES];

/* extern functions */
void add_follower(struct char_data * ch, struct char_data * leader);
ACMD(do_drop);
ACMD(do_gen_door);
ACMD(do_say);

/* local functions */
void sort_spells(void);
int compare_spells(const void *x, const void *y);
const char *how_good(int percent);
void list_skills(struct char_data * ch);
int the_training[][3];
SPECIAL(guild);
SPECIAL(dump);
SPECIAL(mayor);
void npc_steal(struct char_data * ch, struct char_data * victim);
SPECIAL(snake);
SPECIAL(thief);
SPECIAL(magic_user);
SPECIAL(guild_guard);
SPECIAL(puff);
SPECIAL(fido);
SPECIAL(janitor);
SPECIAL(cityguard);
SPECIAL(pet_shops);
SPECIAL(bank);


/* ********************************************************************
*  Special procedures for mobiles                                     *
******************************************************************** */

int spell_sort_info[MAX_SKILLS + 1];

int compare_spells(const void *x, const void *y)
{
  int	a = *(const int *)x,
	b = *(const int *)y;

  return strcmp(spell_info[a].name, spell_info[b].name);
}

void sort_spells(void)
{
  int a;

  /* initialize array, avoiding reserved. */
  for (a = 1; a <= MAX_SKILLS; a++)
    spell_sort_info[a] = a;

  qsort(&spell_sort_info[1], MAX_SKILLS, sizeof(int), compare_spells);
}

const char *how_good(int percent)
{
  if (percent < 0)
    return " error)";
  if (percent == 0)
    return " (not learned)";
  if (percent <= 10)
    return " (awful)";
  if (percent <= 20)
    return " (bad)";
  if (percent <= 40)
    return " (poor)";
  if (percent <= 55)
    return " (average)";
  if (percent <= 70)
    return " (fair)";
  if (percent <= 80)
    return " (good)";
  if (percent <= 85)
    return " (very good)";

  return " (superb)";
}

const char *prac_types[] = {
  "spell",
  "skill"
};

#define LEARNED_LEVEL	0	/* % known which is considered "learned" */
#define MAX_PER_PRAC	1	/* max percent gain in skill per practice */
#define MIN_PER_PRAC	2	/* min percent gain in skill per practice */
#define PRAC_TYPE	3	/* should it say 'spell' or 'skill'?	 */

/* actual prac_params are in class.c */
extern int prac_params[4][NUM_CLASSES];

#define LEARNED(ch) (prac_params[LEARNED_LEVEL][(int)GET_CLASS(ch)])
#define MINGAIN(ch) (prac_params[MIN_PER_PRAC][(int)GET_CLASS(ch)])
#define MAXGAIN(ch) (prac_params[MAX_PER_PRAC][(int)GET_CLASS(ch)])
#define SPLSKL(ch) (prac_types[prac_params[PRAC_TYPE][(int)GET_CLASS(ch)]])

void list_skills(struct char_data * ch)
{
  int i, sortpos;

  if (!GET_PRACTICES(ch))
    strcpy(buf, "You have no practice sessions remaining.\r\n");
  else
    sprintf(buf, "You have %d practice session%s remaining.\r\n",
	    GET_PRACTICES(ch), (GET_PRACTICES(ch) == 1 ? "" : "s"));

  sprintf(buf + strlen(buf), "You know of the following %ss:\r\n", SPLSKL(ch));

  strcpy(buf2, buf);

  for (sortpos = 1; sortpos <= MAX_SKILLS; sortpos++) {
    i = spell_sort_info[sortpos];
    if (strlen(buf2) >= MAX_STRING_LENGTH - 32) {
      strcat(buf2, "**OVERFLOW**\r\n");
      break;
}
    if ((GET_GLEVEL(ch) >= spell_info[i].min_level[(int) GET_CLASS(ch)]) || 
		(GET_SKILL(ch, i) > 0))  {
      sprintf(buf, "%-20s %s\r\n", spell_info[i].name, how_good(GET_SKILL(ch, i)));
      strcat(buf2, buf);	/* The above, ^ should always be safe to do. */
    }
  }

  page_string(ch->desc, buf2, 1);
}


SPECIAL(guild)
{
  int skill_num, percent;

  if (IS_NPC(ch) || !CMD_IS("practice"))
    return (0);

  skip_spaces(&argument);

  if (!*argument) {
    list_skills(ch);
    return (1);
  }
  if (GET_PRACTICES(ch) <= 0) {
    send_to_char("You do not seem to be able to practice now.\r\n", ch);
    return (1);
  }

  skill_num = find_skill_num(argument);

  if (skill_num < 1 ||
      GET_GLEVEL(ch) < spell_info[skill_num].min_level[(int) GET_CLASS(ch)]) {
    sprintf(buf, "You do not know of that %s.\r\n", SPLSKL(ch));
    send_to_char(buf, ch);
    return (1);
  }
  if (GET_SKILL(ch, skill_num) >= LEARNED(ch)) {
    send_to_char("You are already learned in that area.\r\n", ch);
    return (1);
  }
  send_to_char("You practice for a while...\r\n", ch);
  GET_PRACTICES(ch)--;

  percent = GET_SKILL(ch, skill_num);
  percent += MIN(MAXGAIN(ch), MAX(MINGAIN(ch), int_app[GET_INT(ch)].learn));

  SET_SKILL(ch, skill_num, MIN(LEARNED(ch), percent));

  if (GET_SKILL(ch, skill_num) >= LEARNED(ch))
    send_to_char("You are now learned in that area.\r\n", ch);

  return (1);
}



SPECIAL(dump)
{
  struct obj_data *k;
  int value = 0;

  for (k = world[ch->in_room].contents; k; k = world[ch->in_room].contents) {
    act("$p vanishes in a puff of smoke!", FALSE, 0, k, 0, TO_ROOM);
    extract_obj(k);
  }

  if (!CMD_IS("drop"))
    return (0);

  do_drop(ch, argument, cmd, 0);

  for (k = world[ch->in_room].contents; k; k = world[ch->in_room].contents) {
    act("$p vanishes in a puff of smoke!", FALSE, 0, k, 0, TO_ROOM);
    value += MAX(1, MIN(50, GET_OBJ_COST(k) / 10));
    extract_obj(k);
  }

  if (value) {
    send_to_char("You are awarded for outstanding performance.\r\n", ch);
    act("$n has been awarded for being a good citizen.", TRUE, ch, 0, 0, TO_ROOM);

    switch(number(0,2)){
	case 1:
      gain_exp(ch, value);
	  break;
    case 0:
      GET_GOLD(ch) += value;
	  break;
	case 2:break;
	}
  }
  return (1);
}


SPECIAL(mayor)
{
  const char open_path[] =
	"W3a3003b33000c111d0d111Oe333333Oe22c222112212111a1S.";
  const char close_path[] =
	"W3a3003b33000c111d0d111CE333333CE22c222112212111a1S.";

  static const char *path = NULL;
  static int index;
  static bool move = FALSE;

  if (!move) {
    if (time_info.hours == 15) {
      move = TRUE;
      path = open_path;
      index = 0;
    } else if (time_info.hours == 45) {
      move = TRUE;
      path = close_path;
      index = 0;
    }
  }
  if (cmd || !move || (GET_POS(ch) < POS_SLEEPING) ||
      (GET_POS(ch) == POS_FIGHTING))
    return (FALSE);

  switch (path[index]) {
  case '0':
  case '1':
  case '2':
  case '3':
    perform_move(ch, path[index] - '0', 1);
    break;

  case 'W':
    GET_POS(ch) = POS_STANDING;
    act("$n awakens and groans loudly.", FALSE, ch, 0, 0, TO_ROOM);
    break;

  case 'S':
    GET_POS(ch) = POS_SLEEPING;
    act("$n lies down and instantly falls asleep.", FALSE, ch, 0, 0, TO_ROOM);
    break;

  case 'a':
    act("$n says 'Hello Honey!'", FALSE, ch, 0, 0, TO_ROOM);
    act("$n smirks.", FALSE, ch, 0, 0, TO_ROOM);
    break;

  case 'b':
    act("$n says 'What a view!  I must get something done about that dump!'",
	FALSE, ch, 0, 0, TO_ROOM);
    break;

  case 'c':
    act("$n says 'Vandals!  Youngsters nowadays have no respect for anything!'",
	FALSE, ch, 0, 0, TO_ROOM);
    break;

  case 'd':
    act("$n says 'Good day, citizens!'", FALSE, ch, 0, 0, TO_ROOM);
    break;

  case 'e':
    act("$n says 'I hereby declare the bazaar open!'", FALSE, ch, 0, 0, TO_ROOM);
    break;

  case 'E':
    act("$n says 'I hereby declare Mencha closed!'", FALSE, ch, 0, 0, TO_ROOM);
    break;

  case 'O':
    do_gen_door(ch, "gate", 0, SCMD_UNLOCK);
    do_gen_door(ch, "gate", 0, SCMD_OPEN);
    break;

  case 'C':
    do_gen_door(ch, "gate", 0, SCMD_CLOSE);
    do_gen_door(ch, "gate", 0, SCMD_LOCK);
    break;

  case '.':
    move = FALSE;
    break;

  }

  index++;
  return (FALSE);
}


/* ********************************************************************
*  General special procedures for mobiles                             *
******************************************************************** */


void npc_steal(struct char_data * ch, struct char_data * victim)
{
  int gold;

  if (IS_NPC(victim))
    return;
  if (GET_LEVEL(victim) >= LVL_IMMORT)
    return;

  if (AWAKE(victim) && (number(0, 6) == 0)) {
    act("You discover that $n has $s hands in your wallet.", FALSE, ch, 0, victim, TO_VICT);
    act("$n tries to steal gold from $N.", TRUE, ch, 0, victim, TO_NOTVICT);
  } else {
    /* Steal some gold coins */
    gold = (int) ((GET_GOLD(victim) * number(1, 10)) / 100);
    if (gold > 0) {
      GET_GOLD(ch) += gold;
      GET_GOLD(victim) -= gold;
    }
  }
}


SPECIAL(snake)
{
  if (cmd)
    return (FALSE);

  if (GET_POS(ch) != POS_FIGHTING)
    return (FALSE);

  if (FIGHTING(ch) && (FIGHTING(ch)->in_room == ch->in_room) &&
      (number(0, 12) == 0)) {
    act("$n bites $N!", 1, ch, 0, FIGHTING(ch), TO_NOTVICT);
    act("$n bites you!", 1, ch, 0, FIGHTING(ch), TO_VICT);
    call_magic(ch, FIGHTING(ch), 0, SPELL_POISON, GET_LEVEL(ch), CAST_SPELL);
    return (TRUE);
  }
  return (FALSE);
}


SPECIAL(thief)
{
  struct char_data *cons;

  if (cmd)
    return (FALSE);

  if (GET_POS(ch) != POS_STANDING)
    return (FALSE);

  for (cons = world[ch->in_room].people; cons; cons = cons->next_in_room)
    if (!IS_NPC(cons) && (GET_LEVEL(cons) < LVL_IMMORT) && (!number(0, 4))) {
      npc_steal(ch, cons);
      return (TRUE);
    }
  return (FALSE);
}


SPECIAL(magic_user)
{
  struct char_data *vict;

  if (cmd || GET_POS(ch) != POS_FIGHTING)
    return (FALSE);

  /* pseudo-randomly choose someone in the room who is fighting me */
  for (vict = world[ch->in_room].people; vict; vict = vict->next_in_room)
    if (FIGHTING(vict) == ch && !number(0, 4))
      break;

  /* if I didn't pick any of those, then just slam the guy I'm fighting */
  if (vict == NULL && IN_ROOM(FIGHTING(ch)) == IN_ROOM(ch))
    vict = FIGHTING(ch);

  /* Hm...didn't pick anyone...I'll wait a round. */
  if (vict == NULL)
    return (TRUE);

  if ((GET_EXP(ch) > 40000) && (number(0, 10) == 0))
    cast_spell(ch, vict, NULL, SPELL_SLEEP);

  if ((GET_LEVEL(ch) > 20500) && (number(0, 8) == 0))
    cast_spell(ch, vict, NULL, SPELL_BLINDNESS);

  if ((GET_LEVEL(ch) > 35500) && (number(0, 12) == 0)) {
    if (IS_EVIL(ch))
      cast_spell(ch, vict, NULL, SPELL_ENERGY_DRAIN);
    else if (IS_GOOD(ch))
      cast_spell(ch, vict, NULL, SPELL_DISPEL_EVIL);
  }

  if (number(0, 4))
    return (TRUE);

  switch (number(0,20)) {
  case 4:
  case 5:
    cast_spell(ch, vict, NULL, SPELL_MAGIC_MISSILE);
    break;
  case 6:
  case 7:
    cast_spell(ch, vict, NULL, SPELL_CHILL_TOUCH);
    break;
  case 8:
  case 9:
    cast_spell(ch, vict, NULL, SPELL_BURNING_HANDS);
    break;
  case 10:
  case 11:
    cast_spell(ch, vict, NULL, SPELL_SHOCKING_GRASP);
    break;
  case 12:
  case 13:
    cast_spell(ch, vict, NULL, SPELL_LIGHTNING_BOLT);
    break;
  case 14:
  case 15:
  case 16:
  case 17:
    cast_spell(ch, vict, NULL, SPELL_COLOR_SPRAY);
    break;
  default:
    cast_spell(ch, vict, NULL, SPELL_FIREBALL);
    break;
  }
  return (TRUE);

}


/* ********************************************************************
*  Special procedures for mobiles                                      *
******************************************************************** */

SPECIAL(guild_guard)
{
  int i;
  struct char_data *guard = (struct char_data *) me;
  const char *buf = "The guard humiliates you, and blocks your way.\r\n";
  const char *buf2 = "The guard humiliates $n, and blocks $s way.";

  if (!IS_MOVE(cmd) || AFF_FLAGGED(guard, AFF_BLIND))
    return (FALSE);

  if (GET_LEVEL(ch) >= LVL_IMMORT)
    return (FALSE);

  for (i = 0; guild_info[i][0] != -1; i++) {
    if ((IS_NPC(ch) || GET_CLASS(ch) != guild_info[i][0]) &&
	GET_ROOM_VNUM(IN_ROOM(ch)) == guild_info[i][1] &&
	cmd == guild_info[i][2]) {
      send_to_char(buf, ch);
      act(buf2, FALSE, ch, 0, 0, TO_ROOM);
      return (TRUE);
    }
  }

  return (FALSE);
}



SPECIAL(puff)
{
  if (cmd)
    return (0);

  switch (number(0, 60)) {
  case 0:
    do_say(ch, "My god!  It's full of stars!", 0, 0);
    return (1);
  case 1:
    do_say(ch, "How'd all those fish get up here?", 0, 0);
    return (1);
  case 2:
    do_say(ch, "I'm a very female dragon.", 0, 0);
    return (1);
  case 3:
    do_say(ch, "I've got a peaceful, easy feeling.", 0, 0);
    return (1);
  default:
    return (0);
  }
}



SPECIAL(fido)
{

  struct obj_data *i, *temp, *next_obj;

  if (cmd || !AWAKE(ch))
    return (FALSE);

  for (i = world[ch->in_room].contents; i; i = i->next_content) {
    if (IS_CORPSE(i)) {
      act("$n savagely devours a corpse.", FALSE, ch, 0, 0, TO_ROOM);
      for (temp = i->contains; temp; temp = next_obj) {
	next_obj = temp->next_content;
	obj_from_obj(temp);
	obj_to_room(temp, ch->in_room);
      }
      extract_obj(i);
      return (TRUE);
    }
  }
  return (FALSE);
}



SPECIAL(janitor)
{
  struct obj_data *i;

  if (cmd || !AWAKE(ch))
    return (FALSE);

  for (i = world[ch->in_room].contents; i; i = i->next_content) {
    if (!CAN_WEAR(i, ITEM_WEAR_TAKE))
      continue;
    if (GET_OBJ_TYPE(i) != ITEM_DRINKCON && GET_OBJ_COST(i) >= 15)
      continue;
    act("$n picks up some trash.", FALSE, ch, 0, 0, TO_ROOM);
    obj_from_room(i);
    obj_to_char(i, ch);
    return (TRUE);
  }

  return (FALSE);
}


SPECIAL(cityguard)
{
  struct char_data *tch, *evil;
  int max_evil;

  if (cmd || !AWAKE(ch) || FIGHTING(ch))
    return (FALSE);

  max_evil = 1000;
  evil = 0;

  for (tch = world[ch->in_room].people; tch; tch = tch->next_in_room) {
    if (!IS_NPC(tch) && CAN_SEE(ch, tch) && PLR_FLAGGED(tch, PLR_KILLER)) {
      act("$n screams 'HEY!!!  You're one of those PLAYER KILLERS!!!!!!'", FALSE, ch, 0, 0, TO_ROOM);
      hit(ch, tch, TYPE_UNDEFINED);
      return (TRUE);
    }
  }

  for (tch = world[ch->in_room].people; tch; tch = tch->next_in_room) {
    if (!IS_NPC(tch) && CAN_SEE(ch, tch) && PLR_FLAGGED(tch, PLR_THIEF)){
      act("$n screams 'HEY!!!  You're one of those PLAYER THIEVES!!!!!!'", FALSE, ch, 0, 0, TO_ROOM);
      hit(ch, tch, TYPE_UNDEFINED);
      return (TRUE);
    }
  }

  for (tch = world[ch->in_room].people; tch; tch = tch->next_in_room) {
    if (CAN_SEE(ch, tch) && FIGHTING(tch)) {
      if ((GET_ALIGNMENT(tch) < max_evil) &&
	  (IS_NPC(tch) || IS_NPC(FIGHTING(tch)))) {
	max_evil = GET_ALIGNMENT(tch);
	evil = tch;
      }
    }
  }

  if (evil && (GET_ALIGNMENT(FIGHTING(evil)) >= 0)) {
    act("$n screams 'PROTECT THE INNOCENT!  BANZAI!  CHARGE!  ARARARAGGGHH!'", FALSE, ch, 0, 0, TO_ROOM);
    hit(ch, evil, TYPE_UNDEFINED);
    return (TRUE);
  }
  return (FALSE);
}


#define PET_PRICE(pet) (GET_EXP(pet))

SPECIAL(pet_shops)
{
  char buf[MAX_STRING_LENGTH], pet_name[256];
  room_rnum pet_room;
  struct char_data *pet;

  pet_room = ch->in_room + 1;

  if (CMD_IS("list")) {
    send_to_char("Available pets are:\r\n", ch);
    for (pet = world[pet_room].people; pet; pet = pet->next_in_room) {
      sprintf(buf, "%8d - %s\r\n", PET_PRICE(pet), GET_NAME(pet));
      send_to_char(buf, ch);
    }
    return (TRUE);
  } else if (CMD_IS("buy")) {

    two_arguments(argument, buf, pet_name);

    if (!(pet = get_char_room(buf, pet_room))) {
      send_to_char("There is no such pet!\r\n", ch);
      return (TRUE);
    }
    if (GET_GOLD(ch) < PET_PRICE(pet)) {
      send_to_char("You don't have enough gold!\r\n", ch);
      return (TRUE);
    }
    GET_GOLD(ch) -= PET_PRICE(pet);

    pet = read_mobile(GET_MOB_RNUM(pet), REAL);
    GET_EXP(pet) = 0;
    SET_BIT(AFF_FLAGS(pet), AFF_CHARM);

    if (*pet_name) {
      sprintf(buf, "%s %s", pet->player.name, pet_name);
      /* free(pet->player.name); don't free the prototype! */
      pet->player.name = str_dup(buf);

      sprintf(buf, "%sA small sign on a chain around the neck says 'My name is %s'\r\n",
	      pet->player.description, pet_name);
      /* free(pet->player.description); don't free the prototype! */
      pet->player.description = str_dup(buf);
    }
    char_to_room(pet, ch->in_room);
    add_follower(pet, ch);

    /* Be certain that pets can't get/carry/use/wield/wear items */
    IS_CARRYING_W(pet) = 1000;
    IS_CARRYING_N(pet) = 100;

    send_to_char("May you enjoy your pet.\r\n", ch);
    act("$n buys $N as a pet.", FALSE, ch, 0, pet, TO_ROOM);

    return (1);
  }
  /* All commands except list and buy */
  return (0);
}



/* ********************************************************************
*  Special procedures for objects                                     *
******************************************************************** */


SPECIAL(bank)
{
  int amount;

  if (CMD_IS("balance")) {
    if (GET_BANK_GOLD(ch) > 0)
      sprintf(buf, "Your current balance is %d coins.\r\n",
	      GET_BANK_GOLD(ch));
    else
      sprintf(buf, "You currently have no money deposited.\r\n");
    send_to_char(buf, ch);
    return (1);
  } else if (CMD_IS("deposit")) {
    if ((amount = atoi(argument)) <= 0) {
      send_to_char("How much do you want to deposit?\r\n", ch);
      return (1);
    }
    if (GET_GOLD(ch) < amount) {
      send_to_char("You don't have that many coins!\r\n", ch);
      return (1);
    }
    GET_GOLD(ch) -= amount;
    GET_BANK_GOLD(ch) += amount;
    sprintf(buf, "You deposit %d coins.\r\n", amount);
    send_to_char(buf, ch);
    act("$n makes a bank transaction.", TRUE, ch, 0, FALSE, TO_ROOM);
    return (1);
  } else if (CMD_IS("withdraw")) {
    if ((amount = atoi(argument)) <= 0) {
      send_to_char("How much do you want to withdraw?\r\n", ch);
      return (1);
    }
    if (GET_BANK_GOLD(ch) < amount) {
      send_to_char("You don't have that many coins deposited!\r\n", ch);
      return (1);
    }
    GET_GOLD(ch) += amount;
    GET_BANK_GOLD(ch) -= amount;
    sprintf(buf, "You withdraw %d coins.\r\n", amount);
    send_to_char(buf, ch);
    act("$n makes a bank transaction.", TRUE, ch, 0, FALSE, TO_ROOM);
    return (1);
  } else
    return (0);
}


/*STUFF I ADDED!!!*/
SPECIAL(cleric)
{
  int i;
  char buf[MAX_STRING_LENGTH];
  struct char_data *vict;
  struct price_info {
    short int number;
    char name[25];
    short int price;
  } prices[] = {
    /* Spell Num (defined)	Name shown	  Price  */
    { SPELL_REMOVE_POISON, 	"remove poison  ", 500 },
    { SPELL_CURE_BLIND, 	"cure blindness ", 500 },
    { SPELL_CURE_CRITIC, 	"cure critic    ", 700 },
    { SPELL_SANCTUARY, 		"sanctuary      ", 1500 },
    { SPELL_HEAL, 		    "heal           ", 2000 },

    /* The next line must be last, add new spells above. */ 
    { -1, "\r\n", -1 }
  };

/* NOTE:  In interpreter.c, you must define a command called 'heal' for this
   spec_proc to work.  Just define it as do_not_here, and the mob will take 
   care of the rest.  (If you don't know what this means, look in interpreter.c
   for a clue.)
*/

  if (CMD_IS("heal")) {
    argument = one_argument(argument, buf);

    if (GET_POS(ch) == POS_FIGHTING) return TRUE;

    if (*buf) {
      for (i=0; prices[i].number > SPELL_RESERVED_DBC; i++) {
	if (is_abbrev(buf, prices[i].name)) 
	  if (GET_GOLD(ch) < prices[i].price) {
	    act("$n tells you, 'You don't have enough gold for that spell!'",
		FALSE, (struct char_data *) me, 0, ch, TO_VICT);
            return TRUE;
          } else {
	    
	    act("$N gives $n some money.",
		FALSE, (struct char_data *) me, 0, ch, TO_NOTVICT);
	    sprintf(buf, "You give %s %d coins.\r\n", 
		    GET_NAME((struct char_data *) me), prices[i].price);
	    send_to_char(buf, ch);
	    GET_GOLD(ch) -= prices[i].price;
	    /* Uncomment the next line to make the mob get RICH! */
            /* GET_GOLD((struct char_data *) me) += prices[i].price; */

            cast_spell((struct char_data *) me, ch, NULL, prices[i].number);
	    return TRUE;
          
	  }
      }
      act("$n tells you, 'I do not know of that spell!"
	  "  Type 'heal' for a list.'", FALSE, (struct char_data *) me, 
	  0, ch, TO_VICT);
	  
      return TRUE;
    } else {
      act("$n tells you, 'Here is a listing of the prices for my services.'",
	  FALSE, (struct char_data *) me, 0, ch, TO_VICT);
      for (i=0; prices[i].number > SPELL_RESERVED_DBC; i++) {
        sprintf(buf, "%s%d\r\n", prices[i].name, prices[i].price);
        send_to_char(buf, ch);
      }
      return TRUE;
    }
  }

  if (cmd) return FALSE;

  /* pseudo-randomly choose someone in the room */
  for (vict = world[ch->in_room].people; vict; vict = vict->next_in_room)
    if (!number(0, 4))
      break;
  
  /* change the level at the end of the next line to control free spells */
  if (vict == NULL || IS_NPC(vict) || (GET_EXP(vict) > 40000))
    return FALSE;

  switch (number(1, 20)) { 
      case 1: cast_spell(ch, vict, NULL, SPELL_CURE_LIGHT); break; 
      case 2: cast_spell(ch, vict, NULL, SPELL_BLESS); break; 
      case 4: cast_spell(ch, vict, NULL, SPELL_CURE_LIGHT); break; 
      case 5: cast_spell(ch, vict, NULL, SPELL_BLESS); break; 
      case 6: cast_spell(ch, vict, NULL, SPELL_CURE_CRITIC); break;
      case 8: 
	  case 11: cast_spell(ch, vict, NULL, SPELL_CURE_LIGHT); break; 
      case 12: cast_spell(ch, vict, NULL, SPELL_BLESS); break; 
      case 14: cast_spell(ch, vict, NULL, SPELL_CURE_LIGHT); break; 
      case 15: cast_spell(ch, vict, NULL, SPELL_BLESS); break; 
      case 16: cast_spell(ch, vict, NULL, SPELL_CURE_CRITIC); break; 
      case 17: 
      case 18: cast_spell(ch, vict, NULL, SPELL_CURE_CRITIC); break;  
      case 10:
	  case 20: 
        /* special wacky thing, your mileage may vary */ 
	act("$n utters the words, 'energizer'.", TRUE, ch, 0, vict, TO_ROOM);
	act("You feel invigorated!", FALSE, ch, 0, vict, TO_VICT);
	if(IS_MAGIC_USER(ch)){
	GET_MANA(vict) = 
	   MIN(GET_MAX_MANA(vict), MAX((GET_MANA(vict) + 10), number(50, 200)));
        break; 
	}
	else 	if(IS_THIEF(ch) || IS_TRACKER(ch) || IS_MERCHANT(ch) || IS_BARD(ch) || IS_SPY(ch) || IS_GUARD(ch)){
	GET_MOVE(vict) = 
	   MIN(GET_MAX_MOVE(vict), MAX((GET_MOVE(vict) + 10), number(50, 200)));
	break;
	}
	else 	if(IS_KNIGHT(ch) || IS_ASSASSIN(ch) || IS_MERC(ch)){
	GET_HIT(vict) = 
	   MIN(GET_MAX_HIT(vict), MAX((GET_HIT(vict) + 10), number(50, 200)));
	break;
	}
	  default:break;
  }
  return TRUE; 
}               

SPECIAL(trainer)/*FIX ME BRIAN  should be base on exp not trains :) */
{
  if (IS_NPC(ch) || !CMD_IS("train"))
    return 0;

  skip_spaces(&argument);

  if (!*argument) 
  {
    sprintf(buf,
		"Hit:%d Mana:%d Move:%d Str:%d Con:%d Wis:%d Int:%d Dex:%d Cha:%d\r\n",
      GET_MAX_HIT(ch), GET_MAX_MANA(ch), GET_MAX_MOVE(ch), GET_STR(ch), GET_CON(ch), GET_WIS(ch), GET_INT(ch), GET_DEX(ch), GET_CHA(ch));
    sprintf(buf,"%sYou have %d training session",buf, GET_TRAINING(ch));
    if (GET_TRAINING(ch) == 1)
       sprintf(buf,"%s.\r\n",buf);
    else
       sprintf(buf,"%ss.\r\n",buf);
  }

 if (GET_TRAINING(ch) <= 0) {
    send_to_char("You do not seem to be able to train now.\r\n", ch);
    return 1;
  }

  if (strcmp(argument,"hit")==0)
    {
      GET_TRAINING(ch) -=1;
      
	  GET_MAX_HIT(ch) +=number(5,10);
    } else
  if (strcmp(argument,"mana")==0)
    {
      GET_TRAINING(ch) -=1;
      GET_MAX_MANA(ch) +=number(5,10);
    } else
  if (strcmp(argument,"move")==0)
    {
      GET_TRAINING(ch) -=1;
      GET_MAX_MOVE(ch) +=number(5,10);
    } else
  if (strcmp(argument,"str")==0)
    {
      if (GET_STR(ch) >= train_params[0][(int)GET_RACE(ch)])
       { send_to_char("Your are already fully trained there!\r\n",ch); return 1; }
      GET_TRAINING(ch) -=1;
      ch->real_abils.str+=1;
    } else
  if (strcmp(argument,"con")==0)
    {
      if (GET_CON(ch) >= train_params[1][(int)GET_RACE(ch)])
       { send_to_char("Your are already fully trained there!\r\n",ch); return 1; }
      GET_TRAINING(ch) -=1;
      ch->real_abils.con+=1;
    } else
  if (strcmp(argument,"wis")==0)
    {
      if (GET_WIS(ch) >= train_params[2][(int)GET_RACE(ch)])
       { send_to_char("Your are already fully trained there!\r\n",ch); return 1; }
      GET_TRAINING(ch) -=1;
      ch->real_abils.wis+=1;
    } else
  if (strcmp(argument,"int")==0)
    {
      if (GET_INT(ch) >= train_params[3][(int)GET_RACE(ch)])
       { send_to_char("Your are already fully trained there!\r\n",ch); return 1; }
      GET_TRAINING(ch) -=1;
      ch->real_abils.intel+=1;
    } else
  if (strcmp(argument,"dex")==0)
    {
      if (GET_DEX(ch) >= train_params[4][(int)GET_RACE(ch)])
       { send_to_char("Your are already fully trained there!\r\n",ch); return 1; }
      GET_TRAINING(ch) -=1;
      ch->real_abils.dex+=1;
    } else
  if (strcmp(argument,"cha")==0)
    {
      if (GET_CHA(ch) >= train_params[5][(int)GET_RACE(ch)])
       { send_to_char("Your are already fully trained there!\r\n",ch); return 1; }
      GET_TRAINING(ch) -=1;
      ch->real_abils.cha+=1;
    } else
    {
      send_to_char("Train what?\r\n",ch);
      return 1;
    }
  send_to_char("You train for a while...\r\n",ch);
  affect_total(ch);
  return 1;
}

SPECIAL(play_war)
{
  int  wager;
  int  player_card;
  int  dealer_card;
  char buf[128];
  static char *cards[] =
  {"One", "Two", "Three", "Four", "Five", "Six", "Seven",
   "Eight", "Nine", "Ten", "Prince", "Knight", "Wizard"};
  static char *suits[] =
  {"Wands", "Daggers", "Wings", "Fire"};

  if (!CMD_IS("gamble"))
    return 0;
  
  one_argument(argument, arg); /* Get the arg (amount to wager.) */
  
  if (!*arg) {
    send_to_char("How much?\r\n", ch);
    return 1;
  } else wager = atoi(arg) ;
  
  if(wager <= 0){
    send_to_char("Very funny, dick-head.\r\n", ch);
    return 1;
  } else if (wager > GET_GOLD(ch)){
    send_to_char("You don't have that much gold to wager.\r\n", ch);
    return 1;
  } else {
    /*  Okay - gamble away! */
    player_card=number(0, 12);
    dealer_card=number(0, 12);
    sprintf(buf, "You are dealt a %s of %s.\r\n"
                 "The dealer turns up a %s of %s.\r\n",
                 cards[player_card], suits[number(0, 3)],
                 cards[dealer_card], suits[number(0, 3)]);
    send_to_char(buf, ch);
    if(player_card > dealer_card){
      /* You win! */
      sprintf(buf, "You win!  The dealer hands you %d gold coins.\r\n", wager);
      act("$n makes a wager and wins!", FALSE, ch, 0, 0, TO_ROOM);
      send_to_char(buf, ch);
      GET_GOLD(ch) += wager;
    } else if (dealer_card > player_card) {
      /* You lose */
      sprintf(buf, "You lose your wager of %d coins.\r\n"
                   "The dealer takes your gold.\r\n", wager);
      act("$n makes a wager and loses.", FALSE, ch, 0, 0, TO_ROOM);
      send_to_char(buf, ch);
      GET_GOLD(ch) -= wager;
    } else {
      /* WAR! */
      while (player_card==dealer_card) {
        send_to_char("/cRWar!/c0\r\n", ch);
        player_card=number(0, 12);
        dealer_card=number(0, 12);
        sprintf(buf, "You are dealt a %s of %s.\r\n"
                 "The dealer turns of a %s of %s.\r\n",
                 cards[player_card], suits[number(0, 3)],
                 cards[dealer_card], suits[number(0, 3)]);
        send_to_char(buf, ch);
      }
      if(player_card > dealer_card){
        /* You win! */
        sprintf(buf, "You win!  The dealer hands you %d gold "
                     "coins.\r\n", wager);
        act("$n makes a wager and wins!", FALSE, ch, 0, 0, TO_ROOM);
        send_to_char(buf, ch);
        GET_GOLD(ch) += wager;
      } else if (dealer_card > player_card) {
        /* You lose */
        sprintf(buf, "You lose your wager of %d coins.\r\n"
                     "The dealer takes your gold.\r\n", wager);
        act("$n makes a wager and loses.", FALSE, ch, 0, 0, TO_ROOM);
        send_to_char(buf, ch);
        GET_GOLD(ch) -= wager;
      }
    }
    return 1;
  }
}

SPECIAL(play_high_dice)
{
struct char_data *dealer = (struct char_data *) me;
	int bet;
     int die1, die2, die3, die4;

	if(!CMD_IS("gamble"))
		return 0;
	
	one_argument(argument, arg);
	if(!*arg){
		send_to_char("How Much?\r\n", ch);
		return 1;
	}
	else 
		bet = atoi(arg);
	if (bet <= 0){
		send_to_char("Very funny, NOT!!!\r\n",ch);
		return 1;
	}
	
	else if (GET_GOLD(ch) < bet) {
	act("$n says, 'I'm sorry sir but you don't have that much gold.'",
	    FALSE, dealer, 0, ch, TO_VICT);
	return 1;
     } 
	 
	 else if (bet > 5000) {
	act("$n says, 'I'm sorry sir but the limit at this table is 5000 gold pieces.'",
	    FALSE, dealer, 0, ch, TO_VICT);
	return 1;
     } else {
	GET_GOLD(ch) -= bet;
	act("$N places $S bet on the table.", FALSE, 0, 0, ch, TO_NOTVICT);
	act("You place your bet on the table.", FALSE, ch, 0, 0, TO_CHAR);
     }

     /* dealer rolls two dice */
     die1 = number(1, 6);
     die2 = number(1, 6);

     sprintf(buf, "$n rolls the dice, $e gets %d, and %d, for a total of %d.",
	     die1, die2, (die1 + die2));
     act(buf, FALSE, dealer, 0, ch, TO_ROOM);
     /* now its the players turn */
     die3 = number(1, 6);
     die4 = number(1, 6);

     sprintf(buf, "$N rolls the dice, $E gets %d, and %d, for a total of %d.",
	     die3, die4, (die3 + die4));
     act(buf, FALSE, dealer, 0, ch, TO_NOTVICT);
     sprintf(buf, "You roll the dice, and get %d, and %d, for a total of %d.\r\n",
	     die3, die4, (die3 + die4));
     send_to_char(buf, ch);

     if ((die1+die2) >= (die3+die4))
     {
	sprintf(buf, "The house wins %d coins from $N.", bet);
	act(buf, FALSE, dealer, 0, ch, TO_NOTVICT);
	sprintf(buf, "The house wins %d coins from you.\r\n", bet);
	send_to_char(buf, ch);
     } else {
	sprintf(buf, "$N wins %d gold coins!", bet*2);
	act(buf, FALSE, dealer, 0, ch, TO_NOTVICT);
	sprintf(buf, "You win %d gold coins!\r\n", bet*2);
	send_to_char(buf, ch);
	GET_GOLD(ch) += (bet*2);
     }
     return 1;
}


SPECIAL(play_triples)
{
	struct char_data *dealer = (struct char_data *) me;
	int bet;
	char guess[MAX_INPUT_LENGTH];
	char upper[] = "upper";
	char lower[] = "lower";
	char triple[] = "triple";
     int die1, die2, die3, total = 0;

if(!CMD_IS("gamble"))
return 0;

two_arguments(argument, arg, guess);

if (!*arg){
send_to_char("How Much?\r\n", ch);
return 1;
}

bet= atoi(arg);

if(bet <= 0){
	send_to_char("The dealer snaps at you to put away your Monopoly money.",ch);
return 1;
}
     if (GET_GOLD(ch) < bet) {
	act("$n says, 'I'm sorry sir but you don't have that much gold.'",
	    FALSE, dealer, 0, ch, TO_VICT);
	return 1;
     } 
	 
	 else if (bet > 5000) {
	act("$n says, 'I'm sorry sir but the limit at this table is 5000 gold pieces.'",
	    FALSE, dealer, 0, ch, TO_VICT);
	return 1;
     } 
	 
	 else {
	GET_GOLD(ch) -= bet;
	act("$N places $S bet on the table.", FALSE, 0, 0, ch, TO_NOTVICT);
	act("You place your bet on the table.", FALSE, ch, 0, 0, TO_CHAR);
     }

     if (!*guess) {
	act("$n tells you, 'You need to specify upper, lower, or triple!'",
	    FALSE, dealer, 0, ch, TO_VICT);
	act("$n hands your bet back to you.", FALSE, dealer, 0, ch, TO_VICT);
	GET_GOLD(ch) += bet;
	return 1;
     }

    if((strcmp(guess, upper) != 0) && (strcmp(guess, lower) != 0) && (strcmp(guess, triple) != 0)){
	act("$n tells you, 'You need to specify upper, lower, or triple.'",
	    FALSE, dealer, 0, ch, TO_VICT);
	act("$n hands your bet back to you.", FALSE, dealer, 0, ch, TO_VICT);
	GET_GOLD(ch) += bet;
	return 1;
	}

     die1 = number(1, 6);
     die2 = number(1, 6);
     die3 = number(1, 6);

     total = die1 + die2 + die3;

     sprintf(buf, "$N rolls %d, %d, and %d", die1, die2, die3);
     if (die1 == die2 && die2 == die3)
	strcat(buf, ", $E scored a triple!");
     else
	strcat(buf, ".");
     act(buf, FALSE, dealer, 0, ch, TO_NOTVICT);
     sprintf(buf, "You roll %d, %d, and %d, for a total of %d.\r\n", die1,
	     die2, die3, total);
     send_to_char(buf, ch);

     if ((die1 == die2 && die2 == die3) && !strcmp(guess, triple)) {
	/* scored a triple! player wins 3x the bet */
	act("$n says, 'Congratulations $N, you win.'", FALSE, dealer, 0, ch, TO_ROOM);
	sprintf(buf, "$n hands you %d gold pieces.", (bet*3));
	act(buf, FALSE, dealer, 0, ch, TO_VICT);
	act("$n hands $N some gold pieces.", FALSE, dealer, 0, ch, TO_NOTVICT);
	GET_GOLD(ch) += (bet*3);
     } 
	 else if ((total <= 9 && !strcmp(guess, lower))|| (total > 9  && !strcmp(guess, upper))) {
	act("$n says, 'Congratulations $N, you win.'", FALSE, dealer, 0, ch, TO_ROOM);
	sprintf(buf, "$n hands you %d gold pieces.", (bet*2));
	act(buf, FALSE, dealer, 0, ch, TO_VICT);
	act("$n hands $N some gold pieces.", FALSE, dealer, 0, ch, TO_NOTVICT);
	GET_GOLD(ch) += (bet*2);
     } else {
	act("$n says, 'Sorry $N, better luck next time.'", FALSE, dealer, 0, ch, TO_ROOM);
	act("$n greedily counts $s new winnings.", FALSE, dealer, 0, ch, TO_ROOM);
     return 1;
     }
return 1;
}


SPECIAL(play_seven)
{
	struct char_data *dealer = (struct char_data *) me;
	char guess[MAX_INPUT_LENGTH];
	int bet;
     int die1, die2, total = 0;
	
	if(!CMD_IS("gamble"))
		return 0;

two_arguments(argument, arg, guess);

if(!*arg){
	send_to_char("How Much?", ch);
	return 1;
}

	bet= atoi(arg);

if(bet <= 0){
	send_to_char("The dealer snaps at you to put away your Monopoly money.",ch);
return 1;
}

	if (GET_GOLD(ch) < bet) {
	act("$n says, 'I'm sorry sir but you don't have that much gold.'",
	    FALSE, dealer, 0, ch, TO_VICT);
	return 1;
     } else if (bet > 5000) {
	act("$n says, 'I'm sorry sir but the limit at this table is 5000 gold pieces.'",
	    FALSE, dealer, 0, ch, TO_VICT);
	return 1;
     } else {
	GET_GOLD(ch) -= bet;
	act("$N places $S bet on the table.", FALSE, 0, 0, ch, TO_NOTVICT);
	act("You place your bet on the table.", FALSE, ch, 0, 0, TO_CHAR);
     }

     if (!*guess) {
	act("$n tells you, 'You need to specify under, over, or seven.'",
	    FALSE, dealer, 0, ch, TO_VICT);
	act("$n hands your bet back to you.", FALSE, dealer, 0, ch, TO_VICT);
	GET_GOLD(ch) += bet;
	return 1;
     }
     if (strcmp(guess, "under") != 0 && strcmp(guess, "over") != 0 && strcmp(guess, "seven") != 0) {
	act("$n tells you, 'You need to specify under, over, or seven.'",
	    FALSE, dealer, 0, ch, TO_VICT);
	act("$n hands your bet back to you.", FALSE, dealer, 0, ch, TO_VICT);
	GET_GOLD(ch) += bet;
	return 1;
     }

     act("$n says, 'Roll the dice $N and tempt lady luck.'",
	 FALSE, dealer, 0, ch, TO_ROOM);

     die1 = number(1, 6);
     die2 = number(1, 6);
     total = die1 + die2;

     sprintf(buf, "$N rolls the dice, getting a %d and a %d.", die1, die2);
     act(buf, FALSE, dealer, 0, ch, TO_NOTVICT);
     sprintf(buf, "You roll the dice, they come up %d and %d.\r\n", die1, die2);
     send_to_char(buf, ch);

     if ((total < 7 && !strcmp(guess, "under")) || \
	 (total > 7 && !strcmp(guess, "over"))) {
	/* player wins 2x $s money */
	act("$n says, 'Congratulations $N, you win!'", FALSE, dealer, 0, ch, TO_ROOM);
	act("$n hands $N some gold pieces.", FALSE, dealer, 0, ch, TO_NOTVICT);
	sprintf(buf, "$n hands you %d gold pieces.", (bet*2));
	act(buf, FALSE, dealer, 0, ch, TO_VICT);
	GET_GOLD(ch) += (bet*2);
     } else if (total == 7 && !strcmp(guess, "seven")) {
	/* player wins 5x $s money */
	act("$n says, 'Congratulations $N, you win!'", FALSE, dealer, 0, ch, TO_ROOM);
	act("$n hands $N some gold pieces.", FALSE, dealer, 0, ch, TO_NOTVICT);
	sprintf(buf, "$n hands you %d gold pieces.", (bet*5));
	act(buf, FALSE, dealer, 0, ch, TO_VICT);
	GET_GOLD(ch) += (bet*5);
     } else {
	/* player loses */
	act("$n says, 'Sorry $N, you lose.'", FALSE, dealer, 0, ch, TO_ROOM);
	act("$n takes $N's bet from the table.", FALSE, dealer, 0, ch, TO_NOTVICT);
	act("$n takes your bet from the table.", FALSE, dealer, 0, ch, TO_VICT);
     }
     return 1;
}

SPECIAL(play_craps)
{
	struct char_data *dealer = (struct char_data *) me;
	int bet;
     int  die1, die2, mark = 0, last = 0;
     bool won = FALSE, firstime = TRUE;

	if(!CMD_IS("gamble"))
	return 0;
	
one_argument(argument, arg);
	if(!*arg){
		send_to_char("How Much?\r\n", ch);
		return 1;
	}
	else 
		bet = atoi(arg);

if (bet <= 0){
    send_to_char("Very funny, NOT!!!\r\n",ch);
	return 1;
	}

     if (GET_GOLD(ch) < bet) {
	act("$n says, 'I'm sorry sir but you don't have that much gold.'",
	    FALSE, dealer, 0, ch, TO_VICT);
	return 1;
     } else if (bet > 10000) {
	act("$n says, 'I'm sorry sir but the limit at this table is 10000 gold pieces.'",
	    FALSE, dealer, 0, ch, TO_VICT);
	return 1;
     } else {
	GET_GOLD(ch) -= bet;
	act("$N places $S bet on the table.", FALSE, 0, 0, ch, TO_NOTVICT);
	act("You place your bet on the table.", FALSE, ch, 0, 0, TO_CHAR);
     }

     act("$n hands $N the dice and says, 'roll 'em'", FALSE, dealer, 0, ch, TO_NOTVICT);
     act("$n hands you the dice and says, 'roll 'em'", FALSE, dealer, 0, ch, TO_VICT);

     while (won != TRUE) {
       die1 = number(1, 6);
       die2 = number(1, 6);
       mark = die1 + die2;

       sprintf(buf, "$n says, '$N rolls %d and %d, totalling %d.",
	      die1, die2, mark);
       act(buf, FALSE, dealer, 0, ch, TO_ROOM);

       if ((mark == 7  || mark == 11) && firstime) {
	  /* win on first roll of the dice! 3x bet */
	  act("$n says, 'Congratulations $N, you win!'", FALSE, dealer, 0, ch, TO_ROOM);
	  act("$n hands $N some gold pieces.", FALSE, dealer, 0, ch, TO_NOTVICT);
	  sprintf(buf, "$n hands you %d gold pieces.", (bet*3));
	  act(buf, FALSE, dealer, 0, ch, TO_VICT);
	  GET_GOLD(ch) += (bet*3);
	  won = TRUE;
       } else if (mark == 3 || mark == 12 && firstime) {
	  /* player loses on first roll */
	  act("$n says, 'Sorry $N, you lose.'", FALSE, dealer, 0, ch, TO_ROOM);
	  act("$n takes $N's bet from the table.", FALSE, dealer, 0, ch, TO_NOTVICT);
	  act("$n takes your bet from the table.", FALSE, dealer, 0, ch, TO_VICT);
	  won = TRUE;
       } else if ((last == mark) && !firstime && mark != 7 && mark != 11) {
	  /* player makes $s mark! 2x bet */
	  act("$n says, 'Congratulations $N, you win!'", FALSE, dealer, 0, ch, TO_ROOM);
	  act("$n hands $N some gold pieces.", FALSE, dealer, 0, ch, TO_NOTVICT);
	  sprintf(buf, "$n hands you %d gold pieces.", (bet*2));
	  act(buf, FALSE, dealer, 0, ch, TO_VICT);
	  GET_GOLD(ch) += (bet*2);
	  won = TRUE;
       } 
	   else if (mark == 7 || mark == 11) {
		   act("$n says, 'Sorry $N, you lose.'", FALSE, dealer, 0, ch, TO_ROOM);
	  act("$n takes $N's bet from the table.", FALSE, dealer, 0, ch, TO_NOTVICT);
	  act("$n takes your bet from the table.", FALSE, dealer, 0, ch, TO_VICT);
	  won = TRUE;
       }
	   else {
	  sprintf(buf, "$n says, '$N's mark is %d.  Roll 'em again $N!'",
		  mark);
	  act(buf, FALSE, dealer, 0, ch, TO_ROOM);
          firstime = FALSE;
	  last = mark;
	  won = FALSE;
       }
     }
     return 1;
}

SPECIAL(play_slots)
{
     int num1, num2, num3, win = 0;
     char *slot_msg[] = {
	"*YOU SHOULDN'T SEE THIS*",
	"a mithril bar",              /* 1 */
	"a golden dragon",
	"a Dwarven hammer",
	"a temple",
	"an Elven bow",               /* 5 */
	"a red brick",
	"a refuse pile",
	"a waybread",
	"a Gnomish bell",
	"a beggar",                   /* 10 */
     };

if(CMD_IS("play")){
     if (GET_GOLD(ch) < 1) {
	send_to_char("You do not have enough money to play the slots!\r\n", ch);
	return 1;
     } 
	 else
	GET_GOLD(ch) -= 1;

     act("$N pulls on the crank of the Gnomish slot machine.",
	 FALSE, 0, 0, ch, TO_ROOM);
     send_to_char("You pull on the crank of the Gnomish slot machine.\r\n", ch);

     /* very simple roll 3 random numbers from 1 to 10 */
     num1 = number(1, 10);
     num2 = number(1, 10);
     num3 = number(1, 10);

     if (num1 == num2 && num2 == num3) {
	/* all 3 are equal! Woohoo! */
	if (num1 == 1)
	   win += 50;
	else if (num1 == 2)
	   win += 25;
	else if (num1 == 3)
	   win += 15;
	else if (num1 == 4)
	   win += 10;
	else if (num1 == 5)
	   win += 5;
	else if (num1 == 10)
	   win += 1;
     }

     sprintf(buf, "You got %s, %s, %s, ", slot_msg[num1],
	     slot_msg[num2], slot_msg[num3]);
     if (win > 1)
	sprintf(buf, "%syou win %d gold pieces!\r\n", buf, win);
     else if (win == 1)
	sprintf(buf, "%syou win 1 gold piece!\r\n", buf);
     else
	sprintf(buf, "%syou lose.\r\n", buf);
     send_to_char(buf, ch);
     GET_GOLD(ch) += win;

     return 1;
}
else
return(0);

}

SPECIAL(align_guard)
{
  int i;
  struct char_data *guard = (struct char_data *) me;
  const char *buf = "Someone tells you, 'You may not enter there.'\r\n";

  if (!IS_MOVE(cmd))
    return (FALSE);

  if(GET_EXP(ch) <= 17500 && cmd != SCMD_SOUTH){ /*FIX ME BRIAN*/
	  send_to_char("Someone tells you, 'You are too young to declare your aligance.'", ch);
      return (TRUE);
  }

  if (GET_LEVEL(ch) >= LVL_IMMORT)
    return (FALSE);

  for (i = 0; the_training[i][0] != -1; i++) {
    if ((IS_NPC(ch)
		|| (GET_ALIGNMENT(ch) >=  350 && the_training[i][0] == 1) 
		|| (GET_ALIGNMENT(ch) <= -350 && the_training[i][0] == 2)
		|| (GET_ALIGNMENT(ch) >= -350 && GET_ALIGNMENT(ch) <=  350 
		&& the_training[i][0] == 3)) &&	GET_ROOM_VNUM(IN_ROOM(ch)) == guild_info[i][1] 
		&& cmd == guild_info[i][2]) {
      send_to_char(buf, ch);
      return (TRUE);
    }
  }

  return (FALSE);
}

int the_training[][3] = {
/*0 for neutral
  1 for good
  2 for evil*/
  {0, 18702,  SCMD_NORTH},
  {1, 18702,  SCMD_EAST},
  {2, 18702,  SCMD_WEST},
/* this must go last -- add new guards above! */
  {-1, -1, -1}
};

SPECIAL(protect_mage)
{
  int i;
  char buf[MAX_STRING_LENGTH];
  struct char_data *vict;
  struct price_info {
    short int number;
    char name[25];
    short int price;
  } prices[] = {
    /* Spell Num (defined)	Name shown	  Price  */
    { SPELL_ARMOR, 	        "armor          ", 500 },
    { SPELL_REMOVE_CURSE,	"remove curse   ", 2000 },
    { SPELL_STRENGTH,       "strength       ", 750 },
	{ SPELL_FLY,			"fly			", 500 },

    /* The next line must be last, add new spells above. */ 
    { -1, "\r\n", -1 }
  };

/* NOTE:  In interpreter.c, you must define a command called 'heal' for this
   spec_proc to work.  Just define it as do_not_here, and the mob will take 
   care of the rest.  (If you don't know what this means, look in interpreter.c
   for a clue.)
*/

  if (CMD_IS("protect")) {
    argument = one_argument(argument, buf);

    if (GET_POS(ch) == POS_FIGHTING) return TRUE;

    if (*buf) {
      for (i=0; prices[i].number > SPELL_RESERVED_DBC; i++) {
	if (is_abbrev(buf, prices[i].name)) 
	  if (GET_GOLD(ch) < prices[i].price) {
	    act("$n tells you, 'You don't have enough gold for that spell!'",
		FALSE, (struct char_data *) me, 0, ch, TO_VICT);
            return TRUE;
          } else {
	    
	    act("$N gives $n some money.",
		FALSE, (struct char_data *) me, 0, ch, TO_NOTVICT);
	    sprintf(buf, "You give %s %d coins.\r\n", 
		    GET_NAME((struct char_data *) me), prices[i].price);
	    send_to_char(buf, ch);
	    GET_GOLD(ch) -= prices[i].price;
	    /* Uncomment the next line to make the mob get RICH! */
            /* GET_GOLD((struct char_data *) me) += prices[i].price; */

            cast_spell((struct char_data *) me, ch, NULL, prices[i].number);
	    return TRUE;
          
	  }
      }
      act("$n tells you, 'I do not know of that spell!"
	  "  Type 'protect' for a list.'", FALSE, (struct char_data *) me, 
	  0, ch, TO_VICT);
	  
      return TRUE;
    } else {
      act("$n tells you, 'Here is a listing of the prices for my services.'",
	  FALSE, (struct char_data *) me, 0, ch, TO_VICT);
      for (i=0; prices[i].number > SPELL_RESERVED_DBC; i++) {
        sprintf(buf, "%s%d\r\n", prices[i].name, prices[i].price);
        send_to_char(buf, ch);
      }
      return TRUE;
    }
  }

  if (cmd) return FALSE;

  /* pseudo-randomly choose someone in the room */
  for (vict = world[ch->in_room].people; vict; vict = vict->next_in_room)
    if (!number(0, 4))
      break;
  
  /* change the level at the end of the next line to control free spells */
  if (vict == NULL || IS_NPC(vict) || (GET_EXP(vict) > 17500))
    return FALSE;

  switch (number(1, 10)) { 
      case 1: cast_spell(ch, vict, NULL, SPELL_ARMOR); break; 
      case 3: cast_spell(ch, vict, NULL, SPELL_FLY); break; 
      case 5: cast_spell(ch, vict, NULL, SPELL_STRENGTH); break; 
      case 7: cast_spell(ch, vict, NULL, SPELL_FLY); break; 
      case 10: cast_spell(ch, vict, NULL, SPELL_ARMOR); break;
      default:break;
  }
  return TRUE; 
} 