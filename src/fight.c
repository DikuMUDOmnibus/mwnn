/* ************************************************************************
*   File: fight.c                                       Part of CircleMUD *
*  Usage: Combat system                                                   *
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
#include "handler.h"
#include "interpreter.h"
#include "db.h"
#include "spells.h"
#include "screen.h"
#include "constants.h"

#define LEARNED(ch) (prac_params[LEARNED_LEVEL][(int)GET_CLASS(ch)])
#define LEARNED_LEVEL	0	/* % known which is considered "learned" */
/* actual prac_params are in class.c */
extern int prac_params[4][NUM_CLASSES];

/* Structures */
struct char_data *combat_list = NULL;	/* head of l-list of fighting chars */
struct char_data *next_combat_list = NULL;

/* External structures */
extern struct time_info_data time_info;
extern struct index_data *mob_index;
extern struct room_data *world;
extern struct message_list fight_messages[MAX_MESSAGES];
extern struct obj_data *object_list;
extern int pk_allowed;		/* see config.c */
extern int summon_allowed;	/*       "      */
extern int sleep_allowed;       /*       "      */
extern int charm_allowed;       /*       "      */
extern int max_exp_gain;	/* see config.c */
extern int max_exp_loss;	/* see config.c */
extern int max_npc_corpse_time, max_pc_corpse_time;
extern const struct weapon_prof_data wpn_prof[];

extern int rev_dir[];
extern struct index_data *obj_index;
extern struct room_data *world;
 

/* External procedures */
char *fread_action(FILE * fl, int nr);
ACMD(do_flee);
int backstab_mult(int level);
int thaco(int ch_class, int level);
int ok_damage_shopkeeper(struct char_data * ch, struct char_data * victim);
int rage_hit(struct char_data * ch, struct char_data * victim, int dam);

/* local functions */
void perform_group_gain(struct char_data * ch, int base, struct char_data * victim);
void dam_message(int dam, struct char_data * ch, struct char_data * victim, int w_type);
void appear(struct char_data * ch);
void load_messages(void);
void check_killer(struct char_data * ch, struct char_data * vict);
void make_corpse(struct char_data * ch);
void change_alignment(struct char_data * ch, struct char_data * victim);
void death_cry(struct char_data * ch);
void raw_kill(struct char_data * ch);
void die(struct char_data * ch);
void group_gain(struct char_data * ch, struct char_data * victim);
void solo_gain(struct char_data * ch, struct char_data * victim);
char *replace_string(const char *str, const char *weapon_singular, const char *weapon_plural);
void perform_violence(void);
int compute_armor_class(struct char_data *ch);
int compute_thaco(struct char_data *ch);
int race_rage(struct char_data * ch);
void improve_skill(struct char_data *ch, int skill);
int get_weapon_prof(struct char_data *ch, struct obj_data *wield);
void dualhit(struct char_data * ch, struct char_data * victim, int type);

/* Weapon attack texts */
struct attack_hit_type attack_hit_text[] =
{
  {"hit", "hits"},		/* 0 */
  {"sting", "stings"},
  {"whip", "whips"},
  {"slash", "slashes"},
  {"bite", "bites"},
  {"bludgeon", "bludgeons"},	/* 5 */
  {"crush", "crushes"},
  {"pound", "pounds"},
  {"claw", "claws"},
  {"maul", "mauls"},
  {"thrash", "thrashes"},	/* 10 */
  {"pierce", "pierces"},
  {"blast", "blasts"},
  {"punch", "punches"},
  {"stab", "stabs"},
  {"bow", "bow"},
  {"crossbow", "crossbow"},
  {"sling", "sling"},
  {"burn", "roasts"}
};

#define IS_WEAPON(type) (((type) >= TYPE_HIT) && ((type) < TYPE_SUFFERING))

/* The Fight related routines */

void appear(struct char_data * ch)
{
  if (affected_by_spell(ch, SPELL_INVISIBLE))
    affect_from_char(ch, SPELL_INVISIBLE);

  REMOVE_BIT(AFF_FLAGS(ch), AFF_INVISIBLE | AFF_HIDE);

  if (GET_LEVEL(ch) < LVL_IMMORT)
    act("$n slowly fades into existence.", FALSE, ch, 0, 0, TO_ROOM);
  else
    act("You feel a strange presence as $n appears, seemingly from nowhere.",
	FALSE, ch, 0, 0, TO_ROOM);
}


int compute_armor_class(struct char_data *ch)
{
  struct obj_data *wielded = GET_EQ(ch, WEAR_WIELD);
  int armorclass = GET_AC(ch);

  if (AWAKE(ch))
    armorclass += dex_app[GET_DEX(ch)].defensive * 10;
  
 if (!IS_NPC(ch))
   if (wielded && GET_OBJ_TYPE(wielded) == ITEM_WEAPON)
     armorclass += wpn_prof[get_weapon_prof(ch, wielded)].to_ac * 10;

  return (MAX(-100, armorclass));      /* -100 is lowest */
}


void load_messages(void)
{
  FILE *fl;
  int i, type;
  struct message_type *messages;
  char chk[128];

  if (!(fl = fopen(MESS_FILE, "r"))) {
    log("SYSERR: Error reading combat message file %s: %s", MESS_FILE, strerror(errno));
    exit(1);
  }
  for (i = 0; i < MAX_MESSAGES; i++) {
    fight_messages[i].a_type = 0;
    fight_messages[i].number_of_attacks = 0;
    fight_messages[i].msg = 0;
  }


  fgets(chk, 128, fl);
  while (!feof(fl) && (*chk == '\n' || *chk == '*'))
    fgets(chk, 128, fl);

  while (*chk == 'M') {
    fgets(chk, 128, fl);
    sscanf(chk, " %d\n", &type);
    for (i = 0; (i < MAX_MESSAGES) && (fight_messages[i].a_type != type) &&
	 (fight_messages[i].a_type); i++);
    if (i >= MAX_MESSAGES) {
      log("SYSERR: Too many combat messages.  Increase MAX_MESSAGES and recompile.");
      exit(1);
    }
    CREATE(messages, struct message_type, 1);
    fight_messages[i].number_of_attacks++;
    fight_messages[i].a_type = type;
    messages->next = fight_messages[i].msg;
    fight_messages[i].msg = messages;

    messages->die_msg.attacker_msg = fread_action(fl, i);
    messages->die_msg.victim_msg = fread_action(fl, i);
    messages->die_msg.room_msg = fread_action(fl, i);
    messages->miss_msg.attacker_msg = fread_action(fl, i);
    messages->miss_msg.victim_msg = fread_action(fl, i);
    messages->miss_msg.room_msg = fread_action(fl, i);
    messages->hit_msg.attacker_msg = fread_action(fl, i);
    messages->hit_msg.victim_msg = fread_action(fl, i);
    messages->hit_msg.room_msg = fread_action(fl, i);
    messages->god_msg.attacker_msg = fread_action(fl, i);
    messages->god_msg.victim_msg = fread_action(fl, i);
    messages->god_msg.room_msg = fread_action(fl, i);
    fgets(chk, 128, fl);
    while (!feof(fl) && (*chk == '\n' || *chk == '*'))
      fgets(chk, 128, fl);
  }

  fclose(fl);
}


void update_pos(struct char_data * victim)
{
  if ((GET_HIT(victim) > 0) && (GET_POS(victim) > POS_STUNNED))
    return;
  else if (GET_HIT(victim) > 0)
    GET_POS(victim) = POS_STANDING;
  else if (GET_HIT(victim) <= -11)
    GET_POS(victim) = POS_DEAD;
  else if (GET_HIT(victim) <= -6)
    GET_POS(victim) = POS_MORTALLYW;
  else if (GET_HIT(victim) <= -3)
    GET_POS(victim) = POS_INCAP;
  else
    GET_POS(victim) = POS_STUNNED;
}


void check_killer(struct char_data * ch, struct char_data * vict)
{
  char buf[256];

  if (PLR_FLAGGED(vict, PLR_KILLER) || PLR_FLAGGED(vict, PLR_THIEF))
    return;
  if (PLR_FLAGGED(ch, PLR_KILLER) || IS_NPC(ch) || IS_NPC(vict) || ch == vict)
    return;
  if(vict == GET_CHALLENGER(ch))
	  return;
  
  SET_BIT(PLR_FLAGS(ch), PLR_KILLER);
  sprintf(buf, "PC Killer bit set on %s for initiating attack on %s at %s.",
	    GET_NAME(ch), GET_NAME(vict), world[vict->in_room].name);
  mudlog(buf, BRF, LVL_IMMORT, TRUE);
  send_to_char("If you want to be a PLAYER KILLER, so be it...\r\n", ch);
}


/* start one char fighting another (yes, it is horrible, I know... )  */
void set_fighting(struct char_data * ch, struct char_data * vict)
{
  if (ch == vict)
    return;

  if (FIGHTING(ch)) {
    core_dump();
    return;
  }

  ch->next_fighting = combat_list;
  combat_list = ch;

  if (AFF_FLAGGED(ch, AFF_SLEEP))
    affect_from_char(ch, SPELL_SLEEP);

  FIGHTING(ch) = vict;
  GET_POS(ch) = POS_FIGHTING;

  if (!pk_allowed)
    check_killer(ch, vict);
}



/* remove a char from the list of fighting chars */
void stop_fighting(struct char_data * ch)
{
  struct char_data *temp;

  if (ch == next_combat_list)
    next_combat_list = ch->next_fighting;

  REMOVE_FROM_LIST(ch, combat_list, next_fighting);
  ch->next_fighting = NULL;
  FIGHTING(ch) = NULL;
  GET_POS(ch) = POS_STANDING;
  update_pos(ch);
}



void make_corpse(struct char_data * ch)
{
  struct obj_data *corpse, *o;
  struct obj_data *money;
  int i;

  corpse = create_obj();

  corpse->item_number = NOTHING;
  corpse->in_room = NOWHERE;
  corpse->name = str_dup("corpse");

  sprintf(buf2, "The corpse of %s is lying here.", GET_NAME(ch));
  corpse->description = str_dup(buf2);

  sprintf(buf2, "the corpse of %s", GET_NAME(ch));
  corpse->short_description = str_dup(buf2);

  GET_OBJ_TYPE(corpse) = ITEM_CONTAINER;
  GET_OBJ_WEAR(corpse) = ITEM_WEAR_TAKE;
  GET_OBJ_EXTRA(corpse) = ITEM_NODONATE;
  GET_OBJ_VAL(corpse, 0) = 0;	/* You can't store stuff in a corpse */
  GET_OBJ_VAL(corpse, 3) = 1;	/* corpse identifier */
  GET_OBJ_WEIGHT(corpse) = GET_WEIGHT(ch) + IS_CARRYING_W(ch);
  GET_OBJ_RENT(corpse) = 100000;
  if (IS_NPC(ch))
    GET_OBJ_TIMER(corpse) = max_npc_corpse_time;
  else
    GET_OBJ_TIMER(corpse) = max_pc_corpse_time;

  /* transfer character's inventory to the corpse */
  corpse->contains = ch->carrying;
  for (o = corpse->contains; o != NULL; o = o->next_content)
    o->in_obj = corpse;
  object_list_new_owner(corpse, NULL);

  /* transfer character's equipment to the corpse */
  for (i = 0; i < NUM_WEARS; i++)
    if (GET_EQ(ch, i))
      obj_to_obj(unequip_char(ch, i), corpse);

  /* transfer gold */
  if (GET_GOLD(ch) > 0) {
    /* following 'if' clause added to fix gold duplication loophole */
    if (IS_NPC(ch) || (!IS_NPC(ch) && ch->desc)) {
      money = create_money(GET_GOLD(ch));
      obj_to_obj(money, corpse);
    }
    GET_GOLD(ch) = 0;
  }
  ch->carrying = NULL;
  IS_CARRYING_N(ch) = 0;
  IS_CARRYING_W(ch) = 0;

  obj_to_room(corpse, ch->in_room);
}


/* When ch kills victim */
void change_alignment(struct char_data * ch, struct char_data * victim)
{
  /*
   * new alignment change algorithm: if you kill a monster with alignment A,
   * you move 1/16th of the way to having alignment -A.  Simple and fast.
   */
  GET_ALIGNMENT(ch) += (-GET_ALIGNMENT(victim) - GET_ALIGNMENT(ch)) / 16;
}



void death_cry(struct char_data * ch)
{
  int door;

  act("Your blood freezes as you hear $n's death cry.", FALSE, ch, 0, 0, TO_ROOM);

  for (door = 0; door < NUM_OF_DIRS; door++)
    if (CAN_GO(ch, door))
      send_to_room("Your blood freezes as you hear someone's death cry.\r\n",
		world[ch->in_room].dir_option[door]->to_room);
}



void raw_kill(struct char_data * ch)
{
  if (FIGHTING(ch))
    stop_fighting(ch);

  while (ch->affected)
    affect_remove(ch, ch->affected);

  death_cry(ch);

  make_corpse(ch);
  extract_char(ch);
}



void die(struct char_data * ch)
{
  gain_exp(ch, -(GET_S_EXP(ch) / 2));
  if (!IS_NPC(ch))
    REMOVE_BIT(PLR_FLAGS(ch), PLR_KILLER | PLR_THIEF);
  death_cry(ch);
  make_corpse(ch);
  
  if(GET_DRACE(ch) != RACE_GHOST){
	GET_RACE(ch) = RACE_GHOST;
	GET_DEAD(ch) = TRUE;
	send_to_char("You are now a ghost. Go get resurrected.\r\n",ch);
  }

  GET_HIT(ch) = GET_MAX_HIT(ch);
  char_to_room(ch,real_room(GET_HOME(ch)));
  GET_POS(ch) = POS_SLEEPING;  
}



void perform_group_gain(struct char_data * ch, int base,
			     struct char_data * victim)
{
  int share;

  share = MIN(max_exp_gain, MAX(1, base));
  
 
	  if (share > 1) {
    	  sprintf(buf2, "You receive your share of experience -- %d points.\r\n", share);
		  send_to_char(buf2, ch);
  	  } else
  		  send_to_char("You receive your share of experience -- one measly little point!\r\n", ch);

  gain_exp(ch, share);
  change_alignment(ch, victim);
}


void group_gain(struct char_data * ch, struct char_data * victim)
{
  int tot_members, base, tot_gain;
  struct char_data *k;
  struct follow_type *f;

  if (!(k = ch->master))
    k = ch;

  if (AFF_FLAGGED(k, AFF_GROUP) && (k->in_room == ch->in_room))
    tot_members = 1;
  else
    tot_members = 0;

  for (f = k->followers; f; f = f->next)
    if (AFF_FLAGGED(f->follower, AFF_GROUP) && f->follower->in_room == ch->in_room)
      tot_members++;

  /* round up to the next highest tot_members */
  tot_gain = (GET_EXP(victim) / 3) + tot_members - 1;

  /* prevent illegal xp creation when killing players */
  if (!IS_NPC(victim))
    tot_gain = MIN(max_exp_loss * 2 / 3, tot_gain);

  if (tot_members >= 1)
    base = MAX(1, tot_gain / tot_members);
  else
    base = 0;

  if (AFF_FLAGGED(k, AFF_GROUP) && k->in_room == ch->in_room)
    perform_group_gain(k, base, victim);

  for (f = k->followers; f; f = f->next)
    if (AFF_FLAGGED(f->follower, AFF_GROUP) && f->follower->in_room == ch->in_room)
      perform_group_gain(f->follower, base, victim);
}


void solo_gain(struct char_data * ch, struct char_data * victim)
{
  int exp;

  exp = MIN(max_exp_gain, GET_EXP(victim) / 3);
  
  /* Calculate level-difference bonus */
  if (IS_NPC(ch))
    exp += MAX(0, (exp * MIN(4, (GET_EXP(victim) - GET_EXP(ch)))) / 8);
  else{
	  if(GET_EXP(ch) < GET_EXP(victim))
		 exp += MAX(0, (exp * MIN(8, (GET_EXP(victim) - GET_EXP(ch)))) / 8);
  }

  exp = MAX(exp, 1);

  if (exp > 1) {
    sprintf(buf2, "You receive %d experience points.\r\n", exp);
    send_to_char(buf2, ch);
  } else
    send_to_char("You receive one lousy experience point.\r\n", ch);
  
  gain_exp(ch, exp);
  change_alignment(ch, victim);
  
}


char *replace_string(const char *str, const char *weapon_singular, const char *weapon_plural)
{
  static char buf[256];
  char *cp = buf;

  for (; *str; str++) {
    if (*str == '#') {
      switch (*(++str)) {
      case 'W':
	for (; *weapon_plural; *(cp++) = *(weapon_plural++));
	break;
      case 'w':
	for (; *weapon_singular; *(cp++) = *(weapon_singular++));
	break;
      default:
	*(cp++) = '#';
	break;
      }
    } else
      *(cp++) = *str;

    *cp = 0;
  }				/* For */

  return (buf);
}


/* message for doing damage with a weapon */
void dam_message(int dam, struct char_data * ch, struct char_data * victim,
		      int w_type)
{
  char *buf;
  int msgnum;

  static struct dam_weapon_type {
    const char *to_room;
    const char *to_char;
    const char *to_victim;
  } dam_weapons[] = {

    /* use #w for singular (i.e. "slash") and #W for plural (i.e. "slashes") */

    {
      "$n tries to #w $N, but misses.",	/* 0: 0     */
      "You try to #w $N, but miss.",
      "$n tries to #w you, but misses."
    },

    {
      "$n tickles $N as $e #W $M.",	/* 1: 1..2  */
      "You tickle $N as you #w $M.",
      "$n tickles you as $e #W you."
    },

    {
      "$n barely #W $N.",		/* 2: 3..4  */
      "You barely #w $N.",
      "$n barely #W you."
    },

    {
      "$n #W $N.",			/* 3: 5..6  */
      "You #w $N.",
      "$n #W you."
    },

    {
      "$n #W $N hard.",			/* 4: 7..10  */
      "You #w $N hard.",
      "$n #W you hard."
    },

    {
      "$n #W $N very hard.",		/* 5: 11..14  */
      "You #w $N very hard.",
      "$n #W you very hard."
    },

    {
      "$n #W $N extremely hard.",	/* 6: 15..19  */
      "You #w $N extremely hard.",
      "$n #W you extremely hard."
    },

    {
      "$n massacres $N to small fragments with $s #w.",	/* 7: 19..23 */
      "You massacre $N to small fragments with your #w.",
      "$n massacres you to small fragments with $s #w."
    },

    {
      "$n OBLITERATES $N with $s deadly #w!!",	/* 8: > 23   */
      "You OBLITERATE $N with your deadly #w!!",
      "$n OBLITERATES you with $s deadly #w!!"
    }
  };


  w_type -= TYPE_HIT;		/* Change to base of table with text */

  if (dam == 0)		msgnum = 0;
  else if (dam <= 2)    msgnum = 1;
  else if (dam <= 4)    msgnum = 2;
  else if (dam <= 6)    msgnum = 3;
  else if (dam <= 10)   msgnum = 4;
  else if (dam <= 14)   msgnum = 5;
  else if (dam <= 19)   msgnum = 6;
  else if (dam <= 23)   msgnum = 7;
  else			msgnum = 8;

  /* damage message to onlookers */
  buf = replace_string(dam_weapons[msgnum].to_room,
	  attack_hit_text[w_type].singular, attack_hit_text[w_type].plural);
  act(buf, FALSE, ch, NULL, victim, TO_NOTVICT);

  /* damage message to damager */
  send_to_char(CCYEL(ch, C_CMP), ch);
  buf = replace_string(dam_weapons[msgnum].to_char,
	  attack_hit_text[w_type].singular, attack_hit_text[w_type].plural);
  act(buf, FALSE, ch, NULL, victim, TO_CHAR);
  send_to_char(CCNRM(ch, C_CMP), ch);

  /* damage message to damagee */
  send_to_char(CCRED(victim, C_CMP), victim);
  buf = replace_string(dam_weapons[msgnum].to_victim,
	  attack_hit_text[w_type].singular, attack_hit_text[w_type].plural);
  act(buf, FALSE, ch, NULL, victim, TO_VICT | TO_SLEEP);
  send_to_char(CCNRM(victim, C_CMP), victim);
}


/*
 * message for doing damage with a spell or skill
 *  C3.0: Also used for weapon damage on miss and death blows
 */
int skill_message(int dam, struct char_data * ch, struct char_data * vict,
		      int attacktype)
{
  int i, j, nr;
  struct message_type *msg;

  struct obj_data *weap = GET_EQ(ch, WEAR_WIELD);

  for (i = 0; i < MAX_MESSAGES; i++) {
    if (fight_messages[i].a_type == attacktype) {
      nr = dice(1, fight_messages[i].number_of_attacks);
      for (j = 1, msg = fight_messages[i].msg; (j < nr) && msg; j++)
	msg = msg->next;

      if (!IS_NPC(vict) && (GET_LEVEL(vict) >= LVL_IMMORT)) {
	act(msg->god_msg.attacker_msg, FALSE, ch, weap, vict, TO_CHAR);
	act(msg->god_msg.victim_msg, FALSE, ch, weap, vict, TO_VICT);
	act(msg->god_msg.room_msg, FALSE, ch, weap, vict, TO_NOTVICT);
      } else if (dam != 0) {
	if (GET_POS(vict) == POS_DEAD) {
	  send_to_char(CCYEL(ch, C_CMP), ch);
	  act(msg->die_msg.attacker_msg, FALSE, ch, weap, vict, TO_CHAR);
	  send_to_char(CCNRM(ch, C_CMP), ch);

	  send_to_char(CCRED(vict, C_CMP), vict);
	  act(msg->die_msg.victim_msg, FALSE, ch, weap, vict, TO_VICT | TO_SLEEP);
	  send_to_char(CCNRM(vict, C_CMP), vict);

	  act(msg->die_msg.room_msg, FALSE, ch, weap, vict, TO_NOTVICT);
	} else {
	  send_to_char(CCYEL(ch, C_CMP), ch);
	  act(msg->hit_msg.attacker_msg, FALSE, ch, weap, vict, TO_CHAR);
	  send_to_char(CCNRM(ch, C_CMP), ch);

	  send_to_char(CCRED(vict, C_CMP), vict);
	  act(msg->hit_msg.victim_msg, FALSE, ch, weap, vict, TO_VICT | TO_SLEEP);
	  send_to_char(CCNRM(vict, C_CMP), vict);

	  act(msg->hit_msg.room_msg, FALSE, ch, weap, vict, TO_NOTVICT);
	}
      } else if (ch != vict) {	/* Dam == 0 */
	send_to_char(CCYEL(ch, C_CMP), ch);
	act(msg->miss_msg.attacker_msg, FALSE, ch, weap, vict, TO_CHAR);
	send_to_char(CCNRM(ch, C_CMP), ch);

	send_to_char(CCRED(vict, C_CMP), vict);
	act(msg->miss_msg.victim_msg, FALSE, ch, weap, vict, TO_VICT | TO_SLEEP);
	send_to_char(CCNRM(vict, C_CMP), vict);

	act(msg->miss_msg.room_msg, FALSE, ch, weap, vict, TO_NOTVICT);
      }
      return (1);
    }
  }
  return (0);
}

/*
 * Alert: As of bpl14, this function returns the following codes:
 *	< 0	Victim died.
 *	= 0	No damage.
 *	> 0	How much damage done.
 */
int damage(struct char_data * ch, struct char_data * victim, int dam, int attacktype)
{
	  ACMD(do_get);
  ACMD(do_split);
  long local_gold = 0;
  char local_buf[256];
  int block = FALSE;
  bool missile = FALSE;

  if (GET_POS(victim) <= POS_DEAD) {
    log("SYSERR: Attempt to damage corpse '%s' in room #%d by '%s'.",
		GET_NAME(victim), GET_ROOM_VNUM(IN_ROOM(victim)), GET_NAME(ch));
    die(victim);
    return (0);			/* -je, 7/7/92 */
  }
  if (ch->in_room != victim->in_room)
    missile = TRUE;

  /* peaceful rooms */
  if (ch != victim && ROOM_FLAGGED(ch->in_room, ROOM_PEACEFUL)) {
    send_to_char("This room just has such a peaceful, easy feeling...\r\n", ch);
    return (0);
  }

  /* shopkeeper protection */
  if (!ok_damage_shopkeeper(ch, victim))
    return (0);

  /* You can't damage an immortal! */
  if (!IS_NPC(victim) && (GET_LEVEL(victim) >= LVL_IMMORT))
    dam = 0;

  if ((victim != ch) && (!missile)) {
    /* Start the attacker fighting the victim */
    if (GET_POS(ch) > POS_STUNNED && (FIGHTING(ch) == NULL))
      set_fighting(ch, victim);

    /* Start the victim fighting the attacker */
    if (GET_POS(victim) > POS_STUNNED && (FIGHTING(victim) == NULL)) {
      set_fighting(victim, ch);
      if (MOB_FLAGGED(victim, MOB_MEMORY) && !IS_NPC(ch))
	remember(victim, ch);
    }
  }

  /* If you attack a pet, it hates your guts */
  if (victim->master == ch)
    stop_follower(victim);

  /* If the attacker is invisible, he becomes visible */
  if (AFF_FLAGGED(ch, AFF_INVISIBLE | AFF_HIDE))
    appear(ch);
  
  /* Rage hit for now instead of doing in by race..*/
  if(GET_RAGE(ch) >= race_rage(ch) && !IS_NPC(ch)) {
	dam = rage_hit(ch, victim, dam);
	GET_RAGE(ch) -= race_rage(ch);
		}
  
  /* Cut damage in half if victim has sanct, to a minimum 1 */
  if (AFF_FLAGGED(victim, AFF_SANCTUARY) && dam >= 2)
    dam /= 2;

  /* Check for PK if this is not a PK MUD */
  if (!pk_allowed) {
    check_killer(ch, victim);
 /* Daniel Houghton's modification:  Let the PK flag be enough! */
/*    if (PLR_FLAGGED(ch, PLR_KILLER) && (ch != victim))
      dam = 0; */
  }
  
  /* For BLOCK/PARRY/DODGE */
  if (GET_POS(victim)==POS_FIGHTING && !IS_NPC(victim)) {
    if (GET_SKILL(victim, SKILL_PARRY)) {
      if (number(1, 210) <
         GET_SKILL(victim, SKILL_PARRY)+(2*(GET_DEX(victim)-GET_DEX(ch)))) {
        act("You parry $N's vicious attack!", FALSE, victim, 0, ch, TO_CHAR);
        act("$n parries your vicious attack!", FALSE, victim, 0, ch, TO_VICT);
        act("$n parries $N's vicious attack!", FALSE, victim, 0, ch, TO_NOTVICT);
        dam = 0;
		block = TRUE;
		improve_skill(ch, SKILL_PARRY);
      }
    }
  }

  if (GET_POS(victim)==POS_FIGHTING && GET_EQ(victim, WEAR_SHIELD) && block == FALSE && !IS_NPC(victim))  {
    if (GET_SKILL(victim, SKILL_BLOCK)) {
      if (number(1, 320) <
         GET_SKILL(victim, SKILL_BLOCK)+(2*(GET_DEX(victim)-GET_DEX(ch)))) {
        act("You block $N's vicious attack!", FALSE, victim, 0, ch, TO_CHAR);
        act("$n blocks your vicious attack!", FALSE, victim, 0, ch, TO_VICT);
        act("$n blocks $N's vicious attack!", FALSE, victim, 0, ch, TO_NOTVICT);
        dam = 0;
		block = TRUE;
		improve_skill(ch, SKILL_BLOCK);
      }
    }
  }

   if (GET_POS(victim)==POS_FIGHTING && block == FALSE && !IS_NPC(victim)) {
    if (GET_SKILL(victim, SKILL_DODGE)) {
      if (number(1, 640) <
         GET_SKILL(victim, SKILL_DODGE)+(2*(GET_DEX(victim)-GET_DEX(ch)))) {
        act("You dodge $N's vicious attack!", FALSE, victim, 0, ch, TO_CHAR);
        act("$n dodges your vicious attack!", FALSE, victim, 0, ch, TO_VICT);
        act("$n dodges $N's vicious attack!", FALSE, victim, 0, ch, TO_NOTVICT);
        dam = 0;
		block = TRUE;
		improve_skill(ch, SKILL_DODGE);
      }
    }
  }

  /* Set the maximum damage per round and subtract the hit points */
  dam = MAX(MIN(dam, 100), 0);
  GET_HIT(victim) -= dam;

  /* Gain exp for the hit */
  if ((ch != victim) && GET_CLASS(ch) == CLASS_MERC)
    gain_exp(ch, dam);

  update_pos(victim);

  /*
   * skill_message sends a message from the messages file in lib/misc.
   * dam_message just sends a generic "You hit $n extremely hard.".
   * skill_message is preferable to dam_message because it is more
   * descriptive.
   * 
   * If we are _not_ attacking with a weapon (i.e. a spell), always use
   * skill_message. If we are attacking with a weapon: If this is a miss or a
   * death blow, send a skill_message if one exists; if not, default to a
   * dam_message. Otherwise, always send a dam_message.
   */
  if ((!IS_WEAPON(attacktype)) || missile)
    skill_message(dam, ch, victim, attacktype);
  else {
    if (GET_POS(victim) == POS_DEAD || dam == 0) {
      if (!skill_message(dam, ch, victim, attacktype))
	dam_message(dam, ch, victim, attacktype);
    } else {
		dam_message(dam, ch, victim, attacktype);
    }
  }
  
 if(victim == GET_CHALLENGER(ch)  && GET_POS(victim) == POS_DEAD){
		sprintf(buf, "You stay your weapon before the killing blow, forcing %s to surrender.\r\n", GET_NAME(victim));
		send_to_char(buf, ch);
		
		sprintf(buf, "%s's weapon comes down in a death dealing blow.\r\nSuprisingly the blow never comes.  You surrender knowing you have been spared.\r\n", GET_NAME(ch));
		send_to_char(buf, victim);
	
		act("$n forces $N to surrender.", TRUE, ch, 0, victim, TO_NOTVICT);

	GET_AKILLS(ch) += 1;
	GET_ADEATHS(ch) += 1;

	GET_CHALLENGE(ch) = 0;
 	GET_CHALLENGE(victim) = 0;
    GET_HIT(victim) = GET_MAX_HIT(victim);
    GET_MANA(victim) = GET_MAX_MANA(victim);
    GET_MOVE(victim) = GET_MAX_MOVE(victim);

	GET_HIT(ch) = GET_MAX_HIT(ch);
    GET_MANA(ch) = GET_MAX_MANA(ch);
    GET_MOVE(ch) = GET_MAX_MOVE(ch);

	update_pos(victim);
	update_pos(ch);

	FIGHTING(ch) = NULL;

		dam = real_room(3001);
	    
	sprintf(buf, "You are magically transported to the Hall of Heroes.\r\n\r\n");
	send_to_char(buf, ch);
	send_to_char(buf, victim);

	char_from_room(ch);	
	act("$n is transported back to the real world.", TRUE, ch, 0, 0, TO_NOTVICT);
	char_to_room(ch, dam);
	look_at_room(ch, 0);
	act("$n arrives from THE DUAL victorious.", TRUE, ch, 0, 0, TO_NOTVICT);

	char_from_room(victim);
	act("$n is transported back to the real world.", TRUE, victim, 0, 0, TO_NOTVICT);
	char_to_room(victim, dam);
	look_at_room(victim, 0);
	act("$n arrives from THE DUAL cowering.", TRUE, victim, 0, 0, TO_NOTVICT);

	return(-1);
	}

  /* Use send_to_char -- act() doesn't send message if you are DEAD. */
  switch (GET_POS(victim)) {
  case POS_MORTALLYW:
    act("$n is mortally wounded, and will die soon, if not aided.", TRUE, victim, 0, 0, TO_ROOM);
    send_to_char("You are mortally wounded, and will die soon, if not aided.\r\n", victim);
    break;
  case POS_INCAP:
    act("$n is incapacitated and will slowly die, if not aided.", TRUE, victim, 0, 0, TO_ROOM);
    send_to_char("You are incapacitated an will slowly die, if not aided.\r\n", victim);
    break;
  case POS_STUNNED:
    act("$n is stunned, but will probably regain consciousness again.", TRUE, victim, 0, 0, TO_ROOM);
    send_to_char("You're stunned, but will probably regain consciousness again.\r\n", victim);
    break;
  case POS_DEAD:
    act("$n is dead!  R.I.P.", FALSE, victim, 0, 0, TO_ROOM);
    send_to_char("You are dead!  Sorry...\r\n", victim);
    break;

  default:			/* >= POSITION SLEEPING */
    if (dam > (GET_MAX_HIT(victim) / 4))
      send_to_char("That really did HURT!\r\n", victim);

    if (GET_HIT(victim) < (GET_MAX_HIT(victim) / 4)) {
      sprintf(buf2, "%sYou wish that your wounds would stop BLEEDING so much!%s\r\n",
	      CCRED(victim, C_SPR), CCNRM(victim, C_SPR));
      send_to_char(buf2, victim);
      if (ch != victim && MOB_FLAGGED(victim, MOB_WIMPY))
	do_flee(victim, NULL, 0, 0);
    }
    if (!IS_NPC(victim) && GET_WIMP_LEV(victim) && (victim != ch) &&
	GET_HIT(victim) < GET_WIMP_LEV(victim) && GET_HIT(victim) > 0) {
      send_to_char("You wimp out, and attempt to flee!\r\n", victim);
      do_flee(victim, NULL, 0, 0);
    }
    break;
  }

  /* Help out poor linkless people who are attacked */
  if (!IS_NPC(victim) && !(victim->desc) && GET_POS(victim) > POS_STUNNED) {
    do_flee(victim, NULL, 0, 0);
    if (!FIGHTING(victim)) {
      act("$n is rescued by divine forces.", FALSE, victim, 0, 0, TO_ROOM);
      GET_WAS_IN(victim) = victim->in_room;
      char_from_room(victim);
      char_to_room(victim, 0);
    }
  }

  /* stop someone from fighting if they're stunned or worse */
  if ((GET_POS(victim) <= POS_STUNNED) && (FIGHTING(victim) != NULL))
    stop_fighting(victim);

  /* Uh oh.  Victim died. */
  if (GET_POS(victim) == POS_DEAD) {
    if ((ch != victim) && (IS_NPC(victim) || victim->desc)) {
      if (AFF_FLAGGED(ch, AFF_GROUP))
	group_gain(ch, victim);
      else
        solo_gain(ch, victim);
    }

    if (!IS_NPC(victim)) {
      sprintf(buf2, "%s killed by %s at %s", GET_NAME(victim), GET_NAME(ch),
	      world[victim->in_room].name);
      mudlog(buf2, BRF, LVL_IMMORT, TRUE);
	  brag(ch, victim);
      if (MOB_FLAGGED(ch, MOB_MEMORY))
	forget(ch, victim);
    }
    die(victim);
	    /* If Autoloot enabled, get all corpse */
    if (IS_NPC(victim) && !IS_NPC(ch) && PRF_FLAGGED(ch, PRF_AUTOLOOT) && !missile) {  
      do_get(ch,"all corpse",0,0);
    }
    /* If Autoloot AND AutoSplit AND we got money, split with group */
    if (IS_AFFECTED(ch, AFF_GROUP) && (local_gold > 0) &&
        PRF_FLAGGED(ch, PRF_AUTOSPLIT) && PRF_FLAGGED(ch,PRF_AUTOLOOT) && !missile) {
      do_split(ch,local_buf,0,0);   
    }
    
	return (-1);
  }
  return (dam);
}



void hit(struct char_data * ch, struct char_data * victim, int type)
{
  struct obj_data *wielded = GET_EQ(ch, WEAR_WIELD);
  int w_type, victim_ac, calc_thaco, dam, diceroll;
  int wpnprof;

  /* Do some sanity checking, in case someone flees, etc. */
  if (ch->in_room != victim->in_room) {
    if (FIGHTING(ch) && FIGHTING(ch) == victim)
      stop_fighting(ch);
    return;
  }

  /* Find the weapon type (for display purposes only) */
  if (wielded && GET_OBJ_TYPE(wielded) == ITEM_WEAPON)
    w_type = GET_OBJ_VAL(wielded, 3) + TYPE_HIT;
  else {
    if (IS_NPC(ch) && (ch->mob_specials.attack_type != 0))
      w_type = ch->mob_specials.attack_type + TYPE_HIT;
    else
      w_type = TYPE_HIT;
  }

  /* Weapon proficiencies */
 if (!IS_NPC(ch) && wielded && GET_OBJ_TYPE(wielded) == ITEM_WEAPON)
   wpnprof = get_weapon_prof(ch, wielded);
 else
   wpnprof = 0;

  /* Calculate the THAC0 of the attacker */
  if (!IS_NPC(ch))
    calc_thaco = thaco((int) GET_CLASS(ch), (int) GET_LEVEL(ch));
  else		/* THAC0 for monsters is set in the HitRoll */
    calc_thaco = 20;
  calc_thaco -= str_app[STRENGTH_APPLY_INDEX(ch)].tohit;
  calc_thaco -= GET_HITROLL(ch);
  calc_thaco -= wpn_prof[wpnprof].to_hit;
  calc_thaco -= (int) ((GET_INT(ch) - 13) / 1.5);	/* Intelligence helps! */
  calc_thaco -= (int) ((GET_WIS(ch) - 13) / 1.5);	/* So does wisdom */


  /* Calculate the raw armor including magic armor.  Lower AC is better. */
  victim_ac = compute_armor_class(victim) / 10;

  /* roll the die and take your chances... */
  diceroll = number(1, 20);

  /* decide whether this is a hit or a miss */
  if ((((diceroll < 20) && AWAKE(victim)) &&
       ((diceroll == 1) || ((calc_thaco - diceroll) > victim_ac)))) {
    /* the attacker missed the victim */
    if (type == SKILL_BACKSTAB)
      damage(ch, victim, 0, SKILL_BACKSTAB);
    else
      damage(ch, victim, 0, w_type);
  } else {
    /* okay, we know the guy has been hit.  now calculate damage. */

    /* Start with the damage bonuses: the damroll and strength apply */
    dam = str_app[STRENGTH_APPLY_INDEX(ch)].todam;
    dam += GET_DAMROLL(ch);

    /* Maybe holding arrow? */
    if (wielded && GET_OBJ_TYPE(wielded) == ITEM_WEAPON) {
      /* Add weapon-based damage if a weapon is being wielded */
      dam += dice(GET_OBJ_VAL(wielded, 1), GET_OBJ_VAL(wielded, 2));
	  dam += wpn_prof[wpnprof].to_dam;
    } else {
      /* If no weapon, add bare hand damage instead */
      if (IS_NPC(ch)) {
	dam += dice(ch->mob_specials.damnodice, ch->mob_specials.damsizedice);
      } else {
	dam += number(0, 2);	/* Max 2 bare hand damage for players */
      }
    }

    /*
     * Include a damage multiplier if victim isn't ready to fight:
     *
     * Position sitting  1.33 x normal
     * Position resting  1.66 x normal
     * Position sleeping 2.00 x normal
     * Position stunned  2.33 x normal
     * Position incap    2.66 x normal
     * Position mortally 3.00 x normal
     *
     * Note, this is a hack because it depends on the particular
     * values of the POSITION_XXX constants.
     */
    if (GET_POS(victim) < POS_FIGHTING)
      dam *= 1 + (POS_FIGHTING - GET_POS(victim)) / 3;

    /* at least 1 hp damage min per hit */
    dam = MAX(1, dam);

    if (type == SKILL_BACKSTAB) {
      dam *= backstab_mult(GET_GLEVEL(ch));
      damage(ch, victim, dam, SKILL_BACKSTAB);
    } else if (type == SKILL_CIRCLE) {
		dam *= (backstab_mult(GET_GLEVEL(ch))/2);
		damage(ch, victim, dam, SKILL_CIRCLE);
	} else
      damage(ch, victim, dam, w_type);
  }
}



/* control the fights going on.  Called every 2 seconds from comm.c. */
void perform_violence(void)
{
  struct char_data *ch;

  for (ch = combat_list; ch; ch = next_combat_list) {
    next_combat_list = ch->next_fighting;

    if (FIGHTING(ch) == NULL || ch->in_room != FIGHTING(ch)->in_room) {
      stop_fighting(ch);
      continue;
    }

    if (IS_NPC(ch)) {
      if (GET_MOB_WAIT(ch) > 0) {
	GET_MOB_WAIT(ch) -= PULSE_VIOLENCE;
	continue;
      }
      GET_MOB_WAIT(ch) = 0;
      if (GET_POS(ch) < POS_FIGHTING) {
	GET_POS(ch) = POS_FIGHTING;
	act("$n scrambles to $s feet!", TRUE, ch, 0, 0, TO_ROOM);
      }
    }

    if (GET_POS(ch) < POS_FIGHTING) {
      send_to_char("You can't fight while sitting!!\r\n", ch);
      continue;
    }

    hit(ch, FIGHTING(ch), TYPE_UNDEFINED);
	if(!IS_NPC(ch))
	{
		if (GET_SKILL(ch, SKILL_DUAL_WIELD) && FIGHTING(ch) && (GET_SKILL(ch, SKILL_DUAL_WIELD) > number(1, 101))){
			dualhit(ch, FIGHTING(ch), TYPE_UNDEFINED);
			improve_skill(ch, SKILL_DUAL_WIELD);
		}
	
		if(GET_SKILL(ch, SKILL_SECOND) > number(1,101)){
			hit(ch,FIGHTING(ch),TYPE_UNDEFINED);
				improve_skill(ch, SKILL_SECOND);
		
			if (GET_SKILL(ch, SKILL_DUAL_WIELD) && FIGHTING(ch) && (GET_SKILL(ch, SKILL_DUAL_WIELD) > number(1, 101))){
				dualhit(ch, FIGHTING(ch), TYPE_UNDEFINED);
				improve_skill(ch, SKILL_DUAL_WIELD);
			}
		}	
		
		if(GET_SKILL(ch, SKILL_THIRD) > number(1,101)){
			hit(ch,FIGHTING(ch),TYPE_UNDEFINED);
				improve_skill(ch, SKILL_THIRD);
			
			if (GET_SKILL(ch, SKILL_DUAL_WIELD) && FIGHTING(ch) && (GET_SKILL(ch, SKILL_DUAL_WIELD) > number(1, 101))){
				dualhit(ch, FIGHTING(ch), TYPE_UNDEFINED);
				improve_skill(ch, SKILL_DUAL_WIELD);
			}
		}
			
		if(GET_SKILL(ch, SKILL_FOURTH) > number(1, 101)){
			hit(ch,FIGHTING(ch),TYPE_UNDEFINED);
				improve_skill(ch, SKILL_FOURTH);
			
			if (GET_SKILL(ch, SKILL_DUAL_WIELD) && FIGHTING(ch) && (GET_SKILL(ch, SKILL_DUAL_WIELD) > number(1, 101))){
				dualhit(ch, FIGHTING(ch), TYPE_UNDEFINED);
				improve_skill(ch, SKILL_DUAL_WIELD);
			}
		}
	}
    /* XXX: Need to see if they can handle "" instead of NULL. */
    if (MOB_FLAGGED(ch, MOB_SPEC) && mob_index[GET_MOB_RNUM(ch)].func != NULL)
      (mob_index[GET_MOB_RNUM(ch)].func) (ch, ch, 0, "");
  }
}

int race_rage(struct char_data * ch)
{
int rage_max =0;
if(IS_NPC(ch))
   return(-1);

switch(GET_RACE(ch)){
case RACE_ELF:
case RACE_DROW:
	rage_max=200;
	break;
case RACE_WERE:
case RACE_ARIEL:
case RACE_HUMAN:
case RACE_GHOUL:
	rage_max=100;
	break;
case RACE_MINOTAUR:
case RACE_DWARF:
case RACE_GNOME:
	rage_max=75;
	break;
case RACE_VAMPYRE:
case RACE_FAIRY:
case RACE_GHOST:
case RACE_KENDER:
	rage_max=50;
	break;
case RACE_DRACO:
	rage_max=40;
	break;
case RACE_TROLL:
case RACE_GIANT:
	rage_max=25;
	break;
default:
	rage_max=99999;
	log("SYSERR: Missing rage max for race #%i",GET_RACE(ch));
	break;
}
return(rage_max);
}

void improve_skill(struct char_data *ch, int skill)
{
  int percent = GET_SKILL(ch, skill);
  int newpercent;
  char skillbuf[MAX_STRING_LENGTH];

  if (number(1, 200) > GET_WIS(ch) + GET_INT(ch) + GET_SKILL(ch, skill))
     return;
  if (percent == 100 || percent <= 0)
     return;
  newpercent = number(1, 5);
  
  if (percent >= 97)
	  newpercent = 1;

  percent += newpercent;
  SET_SKILL(ch, skill, percent);
  if (newpercent >= 4) {
     sprintf(skillbuf, "You feel your skills improving.\r\n");
     send_to_char(skillbuf, ch);
  }
}

int skill_roll(struct char_data *ch, int skill_num) 
{
  if (number(0, 101) > GET_SKILL(ch, skill_num))
    return FALSE;
  else
    return TRUE;
}


void strike_missile(struct char_data *ch, struct char_data *tch, 
                   struct obj_data *missile, int dir, int attacktype)
{
  int dam;
  
  dam = str_app[STRENGTH_APPLY_INDEX(ch)].todam;
  dam += dice(missile->obj_flags.value[1],
              missile->obj_flags.value[2]); 
  dam += GET_DAMROLL(ch);
         
  
  dir = rev_dir[dir];
  send_to_char("You hit!\r\n", ch);
  sprintf(buf, "$P flies in from the %s and strikes %s.", 
          dirs[dir], GET_NAME(tch));
  act(buf, FALSE, tch, 0, missile, TO_ROOM);
  sprintf(buf, "$P flies in from the %s and hits YOU!",
                dirs[dir]);
  act(buf, FALSE, tch, 0, missile, TO_CHAR);
  
    damage(ch, tch, dam, attacktype);
  return;
} 


void miss_missile(struct char_data *ch, struct char_data *tch, 
                struct obj_data *missile, int dir, int attacktype)
{
	dir = rev_dir[dir];
  sprintf(buf, "$P flies in from the %s and hits the ground!",
                dirs[dir]);
  act(buf, FALSE, tch, 0, missile, TO_ROOM);
  act(buf, FALSE, tch, 0, missile, TO_CHAR);
  send_to_char("You missed!\r\n", ch);
}


void mob_reaction(struct char_data *ch, struct char_data *vict, int dir)
{
  if (IS_NPC(vict) && !FIGHTING(vict) && GET_POS(vict) > POS_STUNNED)

     /* can remember so charge! */
    if (IS_SET(MOB_FLAGS(vict), MOB_MEMORY)) {
      remember(vict, ch);
      sprintf(buf, "$n bellows in pain!");
      act(buf, FALSE, vict, 0, 0, TO_ROOM); 
      if (GET_POS(vict) == POS_STANDING) {
        if (!do_simple_move(vict, rev_dir[dir], 1))
          act("$n stumbles while trying to run!", FALSE, vict, 0, 0, TO_ROOM);
      } else
      GET_POS(vict) = POS_STANDING;
      
    /* can't remember so try to run away */
    } else {
      do_flee(vict, "", 0, 0);
    }
}


void fire_missile(struct char_data *ch, char arg1[MAX_INPUT_LENGTH],
                  struct obj_data *missile, int pos, int range, int dir)
{
  bool shot = FALSE, found = FALSE;
  int attacktype;
  int room, nextroom, distance;
  struct char_data *vict;
    
  if (ROOM_FLAGGED(ch->in_room, ROOM_PEACEFUL)) {
    send_to_char("This room just has such a peaceful, easy feeling...\r\n", ch);
    return;
  }

  room = ch->in_room;

  if CAN_GO2(room, dir)
    nextroom = EXIT2(room, dir)->to_room;
  else
    nextroom = NOWHERE;
  

  if (GET_OBJ_TYPE(missile) == ITEM_GRENADE) {
    send_to_char("You throw it!\r\n", ch);
    sprintf(buf, "$n throws %s %s.", 
      GET_EQ(ch,WEAR_HOLD)->short_description, dirs[dir]);
    act(buf, FALSE, ch, 0, 0, TO_ROOM);
	  dir = rev_dir[dir];
    sprintf(buf, "%s flies in from the %s.\r\n",
            missile->short_description, dirs[dir]);
    send_to_room(buf, nextroom);
    obj_to_room(unequip_char(ch, pos), nextroom);
    return;
  }

  for (distance = 1; ((nextroom != NOWHERE) && (distance <= range)); distance++) {

    for (vict = world[nextroom].people; vict ; vict= vict->next_in_room) {
      if ((isname(arg1, GET_NAME(vict))) && (CAN_SEE(ch, vict))) {
        found = TRUE;
        break;
      }
    }

    if (found == 1) {

      /* Daniel Houghton's missile modification */
      if (missile && ROOM_FLAGGED(vict->in_room, ROOM_PEACEFUL)) {
        send_to_char("Nah.  Leave them in peace.\r\n", ch);
        return;
      }

      switch(GET_OBJ_TYPE(missile)) {
        case ITEM_THROW:
          send_to_char("You throw it!\r\n", ch);
          sprintf(buf, "$n throws %s %s.", GET_EQ(ch,WEAR_HOLD)->short_description,
                       dirs[dir]);
          attacktype = SKILL_THROW;
          break;
        case ITEM_ARROW:
          act("$n aims and fires!", TRUE, ch, 0, 0, TO_ROOM);
          send_to_char("You aim and fire!\r\n", ch);
          attacktype = SKILL_BOW;
          break;
        case ITEM_ROCK:
          act("$n aims and fires!", TRUE, ch, 0, 0, TO_ROOM);
          send_to_char("You aim and fire!\r\n", ch);
          attacktype = SKILL_SLING;
          break;
        case ITEM_BOLT:
          act("$n aims and fires!", TRUE, ch, 0, 0, TO_ROOM);
          send_to_char("You aim and fire!\r\n", ch);
          attacktype = SKILL_CROSSBOW;
          break;
        default:
          attacktype = TYPE_UNDEFINED;
        break;
      }

      if (attacktype != TYPE_UNDEFINED)
        shot = skill_roll(ch, attacktype);
      else
        shot = FALSE;

      if (shot == TRUE) {
        strike_missile(ch, vict, missile, dir, attacktype);
        if ((number(0, 1)) || (attacktype == SKILL_THROW))
          obj_to_char(unequip_char(ch, pos), vict);
        else
          extract_obj(unequip_char(ch, pos));
      } else {
      /* ok missed so move missile into new room */
        miss_missile(ch, vict, missile, dir, attacktype);
        if ((!number(0, 2)) || (attacktype == SKILL_THROW))
          obj_to_room(unequip_char(ch, pos), vict->in_room);
        else
          extract_obj(unequip_char(ch, pos));
      }

      /* either way mob remembers */
      mob_reaction(ch, vict, dir);
      WAIT_STATE(ch, PULSE_VIOLENCE);
      return; 

    } 

    room = nextroom;
    if CAN_GO2(room, dir)
      nextroom = EXIT2(room, dir)->to_room;
    else
      nextroom = NOWHERE;
  }

  send_to_char("Can't find your target!\r\n", ch);
  return;

}


void tick_grenade(void)
{
  struct obj_data *i, *tobj;
  struct char_data *tch, *next_tch;
  int s, t, dam, door;
  /* grenades are activated by pulling the pin - ie, setting the
     one of the extra flag bits. After the pin is pulled the grenade
     starts counting down. once it reaches zero, it explodes. */

  for (i = object_list; i; i = i->next) { 

    if (IS_SET(GET_OBJ_EXTRA(i), ITEM_LIVE_GRENADE)) {
      /* update ticks */
      if (i->obj_flags.value[0] >0)
        i->obj_flags.value[0] -=1;
      else { 
        t = 0;

        /* blow it up */
        /* checks to see if inside containers */
        /* to avoid possible infinite loop add a counter variable */
        s = 0; /* we'll jump out after 5 containers deep and just delete
                       the grenade */

        for (tobj = i; tobj; tobj = tobj->in_obj) {
          s++;
          if (tobj->in_room != NOWHERE) { 
            t = tobj->in_room;
            break;
          } else
            if ((tch = tobj->carried_by)) { 
              t = tch->in_room;
              break;
            } else 
              if ((tch = tobj->worn_by)) { 
                t = tch->in_room;
                break;
              }
          if (s == 5)
            break;
        }

        /* then truly this grenade is nowhere?!? */
        if (t <= 0) {
          sprintf(buf, "serious problem, grenade truly in nowhere\r\n");
          log(buf);
          extract_obj(i);
        } else { /* ok we have a room to blow up */

        /* peaceful rooms */
          if (ROOM_FLAGGED(t, ROOM_PEACEFUL)) {
            sprintf(buf, "You hear %s explode harmlessly, with a loud POP!\n\r", 
              i->short_description);
            send_to_room(buf, t);
            extract_obj(i);
            return;
          }

          dam = dice(i->obj_flags.value[1], i->obj_flags.value[2]);

          sprintf(buf, "Oh no - %s explodes!  KABOOOOOOOOOM!!!\r\n", 
            i->short_description);
          send_to_room(buf, t);

          for (door = 0; door < NUM_OF_DIRS; door++)
            if (CAN_GO2(t, door)) 
              send_to_room("You hear a loud explosion!\r\n", 
              world[t].dir_option[door]->to_room);

          for (tch = world[t].people; tch; tch = next_tch) {
            next_tch= tch->next_in_room;

            if (GET_POS(tch) <= POS_DEAD) {
              log("SYSERR: Attempt to damage a corpse.");
              return;			/* -je, 7/7/92 */
            }

            /* You can't damage an immortal! */
            if (IS_NPC(tch) || (GET_LEVEL(tch) < LVL_IMMORT)) {

              GET_HIT(tch) -= dam;
              act("$n is blasted!", TRUE, tch, 0, 0, TO_ROOM);
              act("You are caught in the blast!", TRUE, tch, 0, 0, TO_CHAR);
              update_pos(tch);

              if (GET_POS(tch) <= POS_DEAD) { 
                make_corpse(tch);
                death_cry(tch);
                extract_char(tch);
              }
            }

          }
               /* ok hit all the people now get rid of the grenade and 
                  any container it might have been in */

          extract_obj(i);

        }
      } /* end else stmt that took care of explosions */    
    } /* end if stmt that took care of live grenades */
  } /* end loop that searches the mud for objects. */

  return;

}

/* Weapon Proficiecies -- (C) 1999 by Fabrizio Baldi */
int get_weapon_prof(struct char_data *ch, struct obj_data *wield)
{
  int value = 0, bonus = 0, learned = 0, type = -1;

  /* No mobs here */
  if (IS_NPC(ch))
    return (bonus);

  /* Check for proficiencies only if characters could have learned it */
  value = GET_OBJ_VAL(wield, 3) + TYPE_HIT;
  switch (value) {
  case TYPE_SLASH:
    type = SKILL_SWORD;
    break;

  case TYPE_STING:
  case TYPE_PIERCE:
  case TYPE_STAB:
    type = SKILL_DAGGER;
    break;

  case TYPE_WHIP:
    type = SKILL_WHIP;
    break;
  
  case TYPE_THRASH:
  case TYPE_CLAW:
    type = SKILL_POLEARM;
    break;

  case TYPE_BLUDGEON:
  case TYPE_MAUL:
  case TYPE_POUND:
  case TYPE_CRUSH:
    type = SKILL_BLUDGEON;
    break;

  case TYPE_BOW:
	  type = SKILL_BOW;
	  break;
  
  case TYPE_CROSSBOW:
	  type = SKILL_CROSSBOW;
	  break;
	  
  case TYPE_SLING:
	  type = SKILL_SLING;
	  break;

  case TYPE_HIT:
  case TYPE_PUNCH:
  case TYPE_BITE:
  case TYPE_BLAST:
    type = SKILL_EXOTIC;
    break;

  default:
    type = -1;
    break;
  }

  if (type != -1) {
    learned = GET_SKILL(ch, type);
    if (learned == 0)
      /*
       * poeple are less effective with weapons that they don't know
       * and have penalities to hitroll & damroll (-2 & -2)
       */
      bonus = 10;
    else if (learned <= 20)
      bonus = 1;
    else if (learned <= 40)
      bonus = 2;
    else if (learned <= 60)
      bonus = 3;
    else if (learned <= 80)
      bonus = 4;
    else if (learned <= 85)
      bonus = 5;
    else if (learned <= 90)
      bonus = 6;
    else if (learned <= 95)
      bonus = 7;
    else if (learned <= 99)
      bonus = 8;
    else
      bonus = 9;
  }
  return (bonus);
}

void dualhit(struct char_data * ch, struct char_data * victim, int type)
{
  struct obj_data *wielded = GET_EQ(ch, WEAR_HOLD);
  int w_type, victim_ac, calc_thaco, dam, diceroll;
  int wpnprof;

  /* Do some sanity checking, in case someone flees, etc. */
  if (ch->in_room != victim->in_room) {
    if (FIGHTING(ch) && FIGHTING(ch) == victim)
      stop_fighting(ch);
    return;
  }

  /* Find the weapon type (for display purposes only) */
  if (wielded && GET_OBJ_TYPE(wielded) == ITEM_WEAPON)
    w_type = GET_OBJ_VAL(wielded, 3) + TYPE_HIT;
  else {
    if (IS_NPC(ch) && (ch->mob_specials.attack_type != 0))
      w_type = ch->mob_specials.attack_type + TYPE_HIT;
    else
      w_type = TYPE_HIT;
  }

  /* Weapon proficiencies */
 if (!IS_NPC(ch) && wielded && GET_OBJ_TYPE(wielded) == ITEM_WEAPON)
   wpnprof = get_weapon_prof(ch, wielded);
 else
   wpnprof = 0;

  /* Calculate the THAC0 of the attacker */
  if (!IS_NPC(ch))
    calc_thaco = thaco((int) GET_CLASS(ch), (int) GET_LEVEL(ch));
  else		/* THAC0 for monsters is set in the HitRoll */
    calc_thaco = 20;
  calc_thaco -= str_app[STRENGTH_APPLY_INDEX(ch)].tohit;
  calc_thaco -= GET_HITROLL(ch);
  calc_thaco -= wpn_prof[wpnprof].to_hit;
  calc_thaco -= (int) ((GET_INT(ch) - 13) / 1.5);	/* Intelligence helps! */
  calc_thaco -= (int) ((GET_WIS(ch) - 13) / 1.5);	/* So does wisdom */


  /* Calculate the raw armor including magic armor.  Lower AC is better. */
  victim_ac = compute_armor_class(victim) / 10;

  /* roll the die and take your chances... */
  diceroll = number(1, 20);

  /* decide whether this is a hit or a miss */
  if ((((diceroll < 20) && AWAKE(victim)) &&
       ((diceroll == 1) || ((calc_thaco - diceroll) > victim_ac)))) {
    /* the attacker missed the victim */
    if (type == SKILL_BACKSTAB)
      damage(ch, victim, 0, SKILL_BACKSTAB);
    else
      damage(ch, victim, 0, w_type);
  } else {
    /* okay, we know the guy has been hit.  now calculate damage. */

    /* Start with the damage bonuses: the damroll and strength apply */
    dam = str_app[STRENGTH_APPLY_INDEX(ch)].todam;
    dam += GET_DAMROLL(ch);

    /* Maybe holding arrow? */
    if (wielded && GET_OBJ_TYPE(wielded) == ITEM_WEAPON) {
      /* Add weapon-based damage if a weapon is being wielded */
      dam += dice(GET_OBJ_VAL(wielded, 1), GET_OBJ_VAL(wielded, 2));
	  dam += wpn_prof[wpnprof].to_dam;
    } else {
      /* If no weapon, add bare hand damage instead */
      if (IS_NPC(ch)) {
	dam += dice(ch->mob_specials.damnodice, ch->mob_specials.damsizedice);
      } else {
	dam += number(0, 2);	/* Max 2 bare hand damage for players */
      }
    }

    /*
     * Include a damage multiplier if victim isn't ready to fight:
     *
     * Position sitting  1.33 x normal
     * Position resting  1.66 x normal
     * Position sleeping 2.00 x normal
     * Position stunned  2.33 x normal
     * Position incap    2.66 x normal
     * Position mortally 3.00 x normal
     *
     * Note, this is a hack because it depends on the particular
     * values of the POSITION_XXX constants.
     */
    if (GET_POS(victim) < POS_FIGHTING)
      dam *= 1 + (POS_FIGHTING - GET_POS(victim)) / 3;

    /* at least 1 hp damage min per hit */
    dam = MAX(1, dam);

	dam = dam * .75;

    if (type == SKILL_BACKSTAB) {
      dam *= backstab_mult(GET_LEVEL(ch));
      damage(ch, victim, dam, SKILL_BACKSTAB);
    } else
      damage(ch, victim, dam, w_type);
  }
}
