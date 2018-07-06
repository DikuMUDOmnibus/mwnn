/* ************************************************************************
*   File: act.offensive.c                               Part of CircleMUD *
*  Usage: player-level commands of an offensive nature                    *
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

/* extern variables */
extern struct room_data *world;
extern struct descriptor_data *descriptor_list;
extern int pk_allowed;
extern char *dirs[];
extern int rev_dir[];

/* extern functions */
void raw_kill(struct char_data * ch);
void check_killer(struct char_data * ch, struct char_data * vict);
int compute_armor_class(struct char_data *ch);
void improve_skill(struct char_data *ch, int skill);
int skill_roll(struct char_data *ch, int skill_num);
void strike_missile(struct char_data *ch, struct char_data *tch, 
                   struct obj_data *missile, int dir, int attacktype);
void miss_missile(struct char_data *ch, struct char_data *tch, 
                struct obj_data *missile, int dir, int attacktype);
void mob_reaction(struct char_data *ch, struct char_data *vict, int dir);
void fire_missile(struct char_data *ch, char arg1[MAX_INPUT_LENGTH],
                  struct obj_data *missile, int pos, int range, int dir);
int rage_hit(struct char_data * ch, struct char_data * victim, int dam);

/* local functions */
ACMD(do_assist);
ACMD(do_hit);
ACMD(do_kill);
ACMD(do_backstab);
ACMD(do_order);
ACMD(do_flee);
ACMD(do_bash);
ACMD(do_rescue);
ACMD(do_kick);
ACMD(do_throw);
ACMD(do_bearhug);


ACMD(do_assist)
{
  struct char_data *helpee, *opponent;

  if (FIGHTING(ch)) {
    send_to_char("You're already fighting!  How can you assist someone else?\r\n", ch);
    return;
  }
  one_argument(argument, arg);

  if (!*arg)
    send_to_char("Whom do you wish to assist?\r\n", ch);
  else if (!(helpee = get_char_vis(ch, arg, FIND_CHAR_ROOM)))
    send_to_char(NOPERSON, ch);
  else if (helpee == ch)
    send_to_char("You can't help yourself any more than this!\r\n", ch);
  else {
    /*
     * Hit the same enemy the person you're helping is.
     */
    if (FIGHTING(helpee))
      opponent = FIGHTING(helpee);
    else
      for (opponent = world[ch->in_room].people;
	   opponent && (FIGHTING(opponent) != helpee);
	   opponent = opponent->next_in_room)
		;

    if (!opponent)
      act("But nobody is fighting $M!", FALSE, ch, 0, helpee, TO_CHAR);
    else if (!CAN_SEE(ch, opponent))
      act("You can't see who is fighting $M!", FALSE, ch, 0, helpee, TO_CHAR);
    else if (!pk_allowed && !IS_NPC(opponent))	/* prevent accidental pkill */
      act("Use 'murder' if you really want to attack $N.", FALSE,
	  ch, 0, opponent, TO_CHAR);
    else {
      send_to_char("You join the fight!\r\n", ch);
      act("$N assists you!", 0, helpee, 0, ch, TO_CHAR);
      act("$n assists $N.", FALSE, ch, 0, helpee, TO_NOTVICT);
      hit(ch, opponent, TYPE_UNDEFINED);
    }
  }
}


ACMD(do_hit)
{
  struct char_data *vict;
  struct follow_type *k;
  ACMD(do_assist);

  one_argument(argument, arg);

  if (!*arg)
    send_to_char("Hit who?\r\n", ch);
  else if (!(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM)))
    send_to_char("They don't seem to be here.\r\n", ch);
  else if (vict == ch) {
    send_to_char("You hit yourself...OUCH!.\r\n", ch);
    act("$n hits $mself, and says OUCH!", FALSE, ch, 0, vict, TO_ROOM);
  } else if (AFF_FLAGGED(ch, AFF_CHARM) && (ch->master == vict))
    act("$N is just such a good friend, you simply can't hit $M.", FALSE, ch, 0, vict, TO_CHAR);
  else {
    if (!pk_allowed && vict != GET_CHALLENGER(ch)) {
      if (!IS_NPC(vict) && !IS_NPC(ch)) {
	if (subcmd != SCMD_MURDER) {
	  send_to_char("Use 'murder' to hit another player.\r\n", ch);
	  return;
	} else {
	  check_killer(ch, vict);
	}
      }
      if (AFF_FLAGGED(ch, AFF_CHARM) && !IS_NPC(ch->master) && !IS_NPC(vict))
	return;			/* you can't order a charmed pet to attack a
				 * player */
    }
    if ((GET_POS(ch) == POS_STANDING) && (vict != FIGHTING(ch))) {
      hit(ch, vict, TYPE_UNDEFINED);
      for (k = ch->followers; k; k=k->next) {
  if (PRF_FLAGGED(k->follower, PRF_AUTOASSIST) || AFF_FLAGGED(k->follower, AFF_CHARM) && 
     (k->follower->in_room == ch->in_room))
	 do_assist(k->follower, GET_NAME(ch), 0, 0);
	  } 
	  WAIT_STATE(ch, PULSE_VIOLENCE + 2);
    } else
      send_to_char("You do the best you can!\r\n", ch);
  }
}



ACMD(do_kill)
{
  struct char_data *vict;

  if ((GET_LEVEL(ch) < LVL_IMPL) || IS_NPC(ch)) {
    do_hit(ch, argument, cmd, subcmd);
    return;
  }
  one_argument(argument, arg);

  if (!*arg) {
    send_to_char("Kill who?\r\n", ch);
  } else {
    if (!(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM)))
      send_to_char("They aren't here.\r\n", ch);
    else if (ch == vict)
      send_to_char("Your mother would be so sad.. :(\r\n", ch);
    else {
      act("You chop $M to pieces!  Ah!  The blood!", FALSE, ch, 0, vict, TO_CHAR);
      act("$N chops you to pieces!", FALSE, vict, 0, ch, TO_CHAR);
      act("$n brutally slays $N!", FALSE, ch, 0, vict, TO_NOTVICT);
      raw_kill(vict);
    }
  }
}



ACMD(do_backstab)
{
  struct char_data *vict;
  int percent, prob;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_BACKSTAB)) {
    send_to_char("You have no idea how to do that.\r\n", ch);
    return;
  }

  one_argument(argument, buf);

  if (!(vict = get_char_vis(ch, buf, FIND_CHAR_ROOM))) {
    send_to_char("Backstab who?\r\n", ch);
    return;
  }
  if (vict == ch) {
    send_to_char("How can you sneak up on yourself?\r\n", ch);
    return;
  }
  if (!GET_EQ(ch, WEAR_WIELD)) {
    send_to_char("You need to wield a weapon to make it a success.\r\n", ch);
    return;
  }
  if (GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 3) != TYPE_PIERCE - TYPE_HIT) {
    send_to_char("Only piercing weapons can be used for backstabbing.\r\n", ch);
    return;
  }
  if (FIGHTING(vict)) {
    send_to_char("You can't backstab a fighting person -- they're too alert!\r\n", ch);
    return;
  }

  if (MOB_FLAGGED(vict, MOB_AWARE) && AWAKE(vict)) {
    act("You notice $N lunging at you!", FALSE, vict, 0, ch, TO_CHAR);
    act("$e notices you lunging at $m!", FALSE, vict, 0, ch, TO_VICT);
    act("$n notices $N lunging at $m!", FALSE, vict, 0, ch, TO_NOTVICT);
    hit(vict, ch, TYPE_UNDEFINED);
    return;
  }

  percent = number(1, 101);	/* 101% is a complete failure */
  prob = GET_SKILL(ch, SKILL_BACKSTAB);

  if (AWAKE(vict) && (percent > prob))
    damage(ch, vict, 0, SKILL_BACKSTAB);
  else  	
    hit(ch, vict, SKILL_BACKSTAB);

  if(!IS_NPC(ch))
	improve_skill(ch, SKILL_BACKSTAB);
  WAIT_STATE(ch, 2 * PULSE_VIOLENCE);
}



ACMD(do_order)
{
  char name[MAX_INPUT_LENGTH], message[MAX_INPUT_LENGTH];
  bool found = FALSE;
  room_rnum org_room;
  struct char_data *vict;
  struct follow_type *k;

  half_chop(argument, name, message);

  if (!*name || !*message)
    send_to_char("Order who to do what?\r\n", ch);
  else if (!(vict = get_char_vis(ch, name, FIND_CHAR_ROOM)) && !is_abbrev(name, "followers"))
    send_to_char("That person isn't here.\r\n", ch);
  else if (ch == vict)
    send_to_char("You obviously suffer from skitzofrenia.\r\n", ch);

  else {
    if (AFF_FLAGGED(ch, AFF_CHARM)) {
      send_to_char("Your superior would not aprove of you giving orders.\r\n", ch);
      return;
    }
    if (vict) {
      sprintf(buf, "$N orders you to '%s'", message);
      act(buf, FALSE, vict, 0, ch, TO_CHAR);
      act("$n gives $N an order.", FALSE, ch, 0, vict, TO_ROOM);

      if ((vict->master != ch) || !AFF_FLAGGED(vict, AFF_CHARM))
	act("$n has an indifferent look.", FALSE, vict, 0, 0, TO_ROOM);
      else {
	send_to_char(OK, ch);
	command_interpreter(vict, message);
      }
    } else {			/* This is order "followers" */
      sprintf(buf, "$n issues the order '%s'.", message);
      act(buf, FALSE, ch, 0, vict, TO_ROOM);

      org_room = ch->in_room;

      for (k = ch->followers; k; k = k->next) {
	if (org_room == k->follower->in_room)
	  if (AFF_FLAGGED(k->follower, AFF_CHARM)) {
	    found = TRUE;
	    command_interpreter(k->follower, message);
	  }
      }
      if (found)
	send_to_char(OK, ch);
      else
	send_to_char("Nobody here is a loyal subject of yours!\r\n", ch);
    }
  }
}



ACMD(do_flee)
{
  int i, attempt, loss;
  struct char_data *was_fighting;

  if (GET_POS(ch) < POS_FIGHTING) {
    send_to_char("You are in pretty bad shape, unable to flee!\r\n", ch);
    return;
  }

  for (i = 0; i < 6; i++) {
    attempt = number(0, NUM_OF_DIRS - 1);	/* Select a random direction */
    if (CAN_GO(ch, attempt) &&
	!ROOM_FLAGGED(EXIT(ch, attempt)->to_room, ROOM_DEATH)) {
      act("$n panics, and attempts to flee!", TRUE, ch, 0, 0, TO_ROOM);
      was_fighting = FIGHTING(ch);
      if (do_simple_move(ch, attempt, TRUE)) {
	send_to_char("You flee head over heels.\r\n", ch);
	if (was_fighting && !IS_NPC(ch)) {
	  loss = GET_MAX_HIT(was_fighting) - GET_HIT(was_fighting);
	  gain_exp(ch, -loss);
	}
      } else {
	act("$n tries to flee, but can't!", TRUE, ch, 0, 0, TO_ROOM);
      }
      return;
    }
  }
  send_to_char("PANIC!  You couldn't escape!\r\n", ch);
}


ACMD(do_bash)
{
  struct char_data *vict;
  int percent, prob;

  one_argument(argument, arg);

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_BASH)) {
    send_to_char("You have no idea how.\r\n", ch);
    return;
  }
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char("This room just has such a peaceful, easy feeling...\r\n", ch);
    return;
  }
  if (!GET_EQ(ch, WEAR_WIELD)) {
    send_to_char("You need to wield a weapon to make it a success.\r\n", ch);
    return;
  }

  if (!(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
    if (FIGHTING(ch) && IN_ROOM(ch) == IN_ROOM(FIGHTING(ch))) {
      vict = FIGHTING(ch);
    } else {
      send_to_char("Bash who?\r\n", ch);
      return;
    }
  }
  if (vict == ch) {
    send_to_char("Aren't we funny today...\r\n", ch);
    return;
  }
  if (AFF_FLAGGED(vict, AFF_FLY)){
	  send_to_char("You can NOT bash a flying person!\r\n", ch);
		  return;
  }
  percent = number(1, 101);	/* 101% is a complete failure */
  prob = GET_SKILL(ch, SKILL_BASH);

  if (MOB_FLAGGED(vict, MOB_NOBASH))
    percent = 101;

  if (percent > prob) {
    damage(ch, vict, 0, SKILL_BASH);
    GET_POS(ch) = POS_SITTING;
  } else {
    /*
     * If we bash a player and they wimp out, they will move to the previous
     * room before we set them sitting.  If we try to set the victim sitting
     * first to make sure they don't flee, then we can't bash them!  So now
     * we only set them sitting if they didn't flee. -gg 9/21/98
     */
    if (damage(ch, vict, 1, SKILL_BASH) > 0) {/* -1 = dead, 0 = miss */
	  if(!IS_NPC(ch))
		improve_skill(ch, SKILL_BASH);
      WAIT_STATE(vict, PULSE_VIOLENCE);
      if (IN_ROOM(ch) == IN_ROOM(vict))
        GET_POS(vict) = POS_SITTING;
    }
  }
  WAIT_STATE(ch, PULSE_VIOLENCE * 2);
}


ACMD(do_rescue)
{
  struct char_data *vict, *tmp_ch;
  int percent, prob;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_RESCUE)) {
    send_to_char("You have no idea how to do that.\r\n", ch);
    return;
  }

  one_argument(argument, arg);

  if (!(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
    send_to_char("Whom do you want to rescue?\r\n", ch);
    return;
  }
  if (vict == ch) {
    send_to_char("What about fleeing instead?\r\n", ch);
    return;
  }
  if (FIGHTING(ch) == vict) {
    send_to_char("How can you rescue someone you are trying to kill?\r\n", ch);
    return;
  }
  for (tmp_ch = world[ch->in_room].people; tmp_ch &&
       (FIGHTING(tmp_ch) != vict); tmp_ch = tmp_ch->next_in_room);

  if (!tmp_ch) {
    act("But nobody is fighting $M!", FALSE, ch, 0, vict, TO_CHAR);
    return;
  }
  percent = number(1, 101);	/* 101% is a complete failure */
  prob = GET_SKILL(ch, SKILL_RESCUE);

  if (percent > prob) {
    send_to_char("You fail the rescue!\r\n", ch);
    return;
  }
  send_to_char("Banzai!  To the rescue...\r\n", ch);
  act("You are rescued by $N, you are confused!", FALSE, vict, 0, ch, TO_CHAR);
  act("$n heroically rescues $N!", FALSE, ch, 0, vict, TO_NOTVICT);

  if (FIGHTING(vict) == tmp_ch)
    stop_fighting(vict);
  if (FIGHTING(tmp_ch))
    stop_fighting(tmp_ch);
  if (FIGHTING(ch))
    stop_fighting(ch);

  set_fighting(ch, tmp_ch);
  set_fighting(tmp_ch, ch);
  if(!IS_NPC(ch))
	improve_skill(ch, SKILL_RESCUE);
  WAIT_STATE(vict, 2 * PULSE_VIOLENCE);
}



ACMD(do_kick)
{
  struct char_data *vict;
  int percent, prob;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_KICK)) {
    send_to_char("You have no idea how.\r\n", ch);
    return;
  }
  one_argument(argument, arg);

  if (!(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
    if (FIGHTING(ch) && IN_ROOM(ch) == IN_ROOM(FIGHTING(ch))) {
      vict = FIGHTING(ch);
    } else {
      send_to_char("Kick who?\r\n", ch);
      return;
    }
  }
  if (vict == ch) {
    send_to_char("Aren't we funny today...\r\n", ch);
    return;
  }
  /* 101% is a complete failure */
  percent = ((10 - (compute_armor_class(vict) / 10)) * 2) + number(1, 101);
  prob = GET_SKILL(ch, SKILL_KICK);

  if (percent > prob) {
    damage(ch, vict, 0, SKILL_KICK);
  } else
    damage(ch, vict, GET_STR(ch), SKILL_KICK);
  if(!IS_NPC(ch))
	improve_skill(ch, SKILL_KICK);
  WAIT_STATE(ch, PULSE_VIOLENCE * 3);
}

int chance(int num)
{
    if (number(1,100) <= num) return 1;
    else return 0;
}

ACMD(do_bearhug)
{
  struct char_data *vict;
  int percent, prob;

  one_argument(argument, arg);

  if (!(vict = get_char_room_vis(ch, arg))) {
    if (FIGHTING(ch)) {
      vict = FIGHTING(ch);
    } else {
      send_to_char("Bearhug who?\r\n", ch);
      return;
    }
  }
  if (vict == ch) {
    send_to_char("Aren't we funny today...\r\n", ch);
    return;
  }
  percent = ((10 - (GET_AC(vict) / 10)) << 1) + number(1, 101);
  prob = GET_SKILL(ch, SKILL_BEARHUG);

  if (MOB_FLAGGED(vict, MOB_NOBEARHUG)) {
    percent = 101;
  }

  if (percent > prob) {
    damage(ch, vict, 0, SKILL_BEARHUG);
  } else
    damage(ch, vict, number(1, 25) + GET_STR(ch), SKILL_BEARHUG);
  if(!IS_NPC(ch))
  	improve_skill(ch, SKILL_BEARHUG);
  WAIT_STATE(ch, PULSE_VIOLENCE * 3);
}

ACMD(do_shoot)
{ 
  struct obj_data *missile;
  char arg2[MAX_INPUT_LENGTH];
  char arg1[MAX_INPUT_LENGTH];
  int dir, range;

  if (!GET_EQ(ch, WEAR_WIELD)) { 
    send_to_char("You aren't wielding a shooting weapon!\r\n", ch);
    return;
  }

  if (!GET_EQ(ch, WEAR_HOLD)) { 
    send_to_char("You need to be holding a missile!\r\n", ch);
    return;
  }

  missile = GET_EQ(ch, WEAR_HOLD);

  if ((GET_OBJ_TYPE(GET_EQ(ch, WEAR_WIELD)) == ITEM_SLING) &&
      (GET_OBJ_TYPE(missile) == ITEM_ROCK))
       range = GET_EQ(ch, WEAR_WIELD)->obj_flags.value[0];
  else 
    if ((GET_OBJ_TYPE(missile) == ITEM_ARROW) &&
        (GET_OBJ_TYPE(GET_EQ(ch, WEAR_WIELD)) == ITEM_BOW))
         range = GET_EQ(ch, WEAR_WIELD)->obj_flags.value[0];
  else 
    if ((GET_OBJ_TYPE(missile) == ITEM_BOLT) &&
        (GET_OBJ_TYPE(GET_EQ(ch, WEAR_WIELD)) == ITEM_CROSSBOW))
         range = GET_EQ(ch, WEAR_WIELD)->obj_flags.value[0];

  else {
    send_to_char("You should wield a missile weapon and hold a missile!\r\n", ch);
    return;
  }

  two_arguments(argument, arg1, arg2);

  if (!*arg1 || !*arg2) {
    send_to_char("You should try: shoot <someone> <direction>\r\n", ch);
    return;
  }

/*  if (IS_DARK(ch->in_room)) { 
    send_to_char("You can't see that far.\r\n", ch);
    return;
  } */

  if ((dir = search_block(arg2, dirs, FALSE)) < 0) {
    send_to_char("What direction?\r\n", ch);
    return;
  }

  if (!CAN_GO(ch, dir)) { 
    send_to_char("Something blocks the way!\r\n", ch);
    return;
  }

  if (range > 3) 
    range = 3;
  if (range < 1)
    range = 1;

  fire_missile(ch, arg1, missile, WEAR_HOLD, range, dir);

}
ACMD(do_throw)
{
/*  sample format: throw monkey east
 this would throw a throwable or grenade object wielded
 into the room 1 east of the pc's current room. The chance
 to hit the monkey would be calculated based on the pc's skill.
 if the wielded object is a grenade then it does not 'hit' for
 damage, it is merely dropped into the room. (the timer is set
 with the 'pull pin' command.) */
   extern struct str_app_type str_app[];
   struct obj_data *missile;
   struct char_data *vict;
  int dir, range = 1;   char arg2[MAX_INPUT_LENGTH];   char arg1[MAX_INPUT_LENGTH];
    int percent, prob, dam;
	   two_arguments(argument, arg1, arg2);

 
/* only two types of throwing objects: 
   ITEM_THROW - knives, stones, etc
   ITEM_GRENADE - calls tick_grenades.c . */
  
  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_THROW)) {
    send_to_char("You have no idea how.\r\n", ch);
    return;
  }

  if (!(GET_EQ(ch, WEAR_HOLD))) { 
    send_to_char("You should hold something first!\r\n", ch);
    return;
  }

  missile = GET_EQ(ch, WEAR_HOLD);
  
  if (!((GET_OBJ_TYPE(missile) == ITEM_THROW) ||
     (GET_OBJ_TYPE(missile) == ITEM_GRENADE))) { 
    send_to_char("You should hold a throwing weapon first!\r\n", ch);
     return;
  }
  
  if (GET_OBJ_TYPE(missile) == ITEM_GRENADE) {
    if (!*arg1) {
      send_to_char("You should try: throw <direction>\r\n", ch);
      return;
    }
    if ((dir = search_block(arg1, dirs, FALSE)) < 0) { 
      send_to_char("What direction?\r\n", ch);
      return;
    }
  fire_missile(ch, arg1, missile, WEAR_HOLD, range, dir);
  } else {

    two_arguments(argument, arg1, arg2);

    if (!*arg1 && !*arg2) {
      send_to_char("You should try: throw <someone> <direction> or throw <target>\r\nif in the same room!!\r\n", ch);
      return;
    }
if (*arg1 && !*arg2){

 if (!(vict = get_char_room_vis(ch, arg1))) {
    send_to_char("Throw at whom?\r\n", ch);
    return;
  }
   percent = number(1, 101);	
  prob = GET_SKILL(ch, SKILL_THROW);

    if (percent > prob) {
      damage(ch, vict, 0, SKILL_THROW);
              act("$N throws $p at you and misses by a long shot.", FALSE, vict, missile, ch, TO_CHAR);
             act("You throw $p at $n but, miss by a long shot.", FALSE, vict, missile, ch, TO_VICT);
            act("$N throws $p at $n but, misses by a long shot.", FALSE, vict, missile, ch, TO_NOTVICT);
      return;
  }
	
      else {
        act("$N throws $p and hits you square in the chest.", FALSE, vict, missile, ch, TO_CHAR);
             act("You throw $p at $n and hit $m in the chest.", FALSE, vict, missile, ch, TO_VICT);
             act("$N throws $p at $n and hits $m in the chest.", FALSE, vict, missile, ch, TO_NOTVICT);
      }

  dam = str_app[STRENGTH_APPLY_INDEX(ch)].todam;
  dam += dice(missile->obj_flags.value[1],
              missile->obj_flags.value[2]); 
  dam += GET_DAMROLL(ch);
  
  damage(ch, vict, dam, SKILL_THROW);
  WAIT_STATE(ch, PULSE_VIOLENCE * 3);
  extract_obj(missile);

	} else {
/* arg2 must be a direction */

    if ((dir = search_block(arg2, dirs, FALSE)) < 0) { 
      send_to_char("What direction?\r\n", ch);
      return;
    }
  
/* make sure we can go in the direction throwing. */
  if (!CAN_GO(ch, dir)) { 
    send_to_char("Something blocks the way!\r\n", ch);
    return;
  }

  fire_missile(ch, arg1, missile, WEAR_HOLD, range, dir);
	}
}
}

int rage_hit(struct char_data * ch, struct char_data * victim, int dam){

switch(GET_RACE(ch)){
case RACE_HUMAN:
	switch(number(1,10)){
	case 1:
	act("$n begins to sob uncontrollable and fumbles $s weapon!!", TRUE, ch, 0, victim, TO_ROOM);
	act("$n begins to sob uncontrollable and fumbles $s weapon!!", FALSE, ch, 0, victim, TO_VICT);
	act("The stress becomes to much and you breakdown causing you to fumble your weapon.", FALSE, ch, 0,victim,TO_CHAR);
	dam = 0;
	break;
	default:
	act("$n's face reddens with RAGE, as $e hits $N!", TRUE, ch, 0, victim, TO_ROOM);
	act("$n's face glows with RAGE as $e pommels you!", FALSE, ch, 0, victim, TO_VICT);
	act("You pound $N using all your might.", FALSE, ch, 0, victim, TO_CHAR);
	dam = dam * 1.5;
	break;
	}
	return dam;
case RACE_ELF:
case RACE_DROW:
	switch(number(1,3)){
	case 1:
	act("$n eyes glaze over and $s fighting style seems to lag.", TRUE, ch, 0, victim, TO_ROOM);
	act("$n eyes glaze over and $s fighting style seems to lag, allowing you an easy dodge.", TRUE, ch, 0, victim, TO_VICT);
	act("You linger on your agner and seem to slow down in battle.", TRUE, ch, 0, victim, TO_CHAR);
	dam = 0;
	break;
	default:	
	act("$n glares at $N, fire seems to to build behind $s eyes.", TRUE, ch, 0, victim, TO_ROOM);
	act("$n glares at you.  The fire in $s eyes seems to make his hit hurt worse.", FALSE, ch,  0, victim, TO_VICT);
	act("Your anger builds and you take it out with a fierce hit to $N.", FALSE, ch, 0, victim, TO_CHAR);
	dam = dam * 1.1;
	break;
	}
	return dam;
case RACE_ARIEL:
	switch(number(1, 15)){
	case 1:
	act("$n wraps $s body in $s wings, to hide $s face.", TRUE, ch, 0, victim, TO_ROOM);
	act("$n wraps $s body in $s wings, to hide $s face.", TRUE, ch, 0, victim, TO_VICT);
	act("You wrap your body in your wings, to hide your face.", TRUE, ch, 0, victim, TO_CHAR);
	dam = 0;
	break;
	default:
	act("$n flies into the air and screams loudly!", TRUE, ch, 0, victim, TO_ROOM);
	act("$n flies into the air and screams loudly!", TRUE, ch, 0, victim, TO_VICT);
	act("You fly into the air and screams loudly!", TRUE, ch, 0, victim, TO_CHAR);
	SET_BIT(AFF_FLAGS(ch), AFF_FLY);
	break;
	}
	return dam;
default:
		act("$n goes completly enraged and hits you!", TRUE, ch, 0, victim, TO_ROOM);
	act("$n goes completly enraged and hits you!", TRUE, ch, 0, victim, TO_VICT);
	act("You go completly enraged and hits $N!", TRUE, ch, 0, victim, TO_CHAR);
dam = dam * 1.75;
break;
}
return(dam);
}

ACMD(do_charge)
{
  struct char_data *vict;
  int percent, prob;

  one_argument(argument, arg);

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_CHARGE)) {
    send_to_char("You have no idea how.\r\n", ch);
    return;
  }
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char("This room just has such a peaceful, easy feeling...\r\n", ch);
    return;
  }
 if (!RIDING(ch) || ((GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 3) != TYPE_THRASH - TYPE_HIT) || (GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 3) != TYPE_CLAW - TYPE_HIT))) {
    send_to_char("You need to be mounted and weilding a polearm to attempt charging.\r\n", ch);
    return;
  }

  if (!(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
    if (FIGHTING(ch) && IN_ROOM(ch) == IN_ROOM(FIGHTING(ch))) {
      vict = FIGHTING(ch);
    } else {
      send_to_char("Charge who?\r\n", ch);
      return;
    }
  }
  if (vict == ch) {
    send_to_char("Aren't we funny today...\r\n", ch);
    return;
  }
  if (AFF_FLAGGED(vict, AFF_FLY)){
	  send_to_char("You can NOT charge a flying person!\r\n", ch);
		  return;
  }
  percent = number(1, 101);	/* 101% is a complete failure */
  prob = GET_SKILL(ch, SKILL_CHARGE);

  if (percent > prob) {
    damage(ch, vict, 0, SKILL_CHARGE);
    GET_POS(ch) = POS_SITTING;
  } else {
    /*
     * If we bash a player and they wimp out, they will move to the previous
     * room before we set them sitting.  If we try to set the victim sitting
     * first to make sure they don't flee, then we can't bash them!  So now
     * we only set them sitting if they didn't flee. -gg 9/21/98
     */
    if (damage(ch, vict, 1, SKILL_CHARGE) > 0) {/* -1 = dead, 0 = miss */
	  if(!IS_NPC(ch))
		improve_skill(ch, SKILL_CHARGE);
      WAIT_STATE(vict, PULSE_VIOLENCE);
      if (IN_ROOM(ch) == IN_ROOM(vict))
        GET_POS(vict) = POS_SITTING;
    }
  }
  WAIT_STATE(ch, PULSE_VIOLENCE * 2);
}

ACMD(do_circle)
{
  struct char_data *vict;
  int percent, prob;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_CIRCLE)) {
    send_to_char("You have no idea how to do that.\r\n", ch);
    return;
  }

  if (FIGHTING(ch) && IN_ROOM(ch) == IN_ROOM(FIGHTING(ch))) {
     vict = FIGHTING(ch);
  } else {
	send_to_char("You must be fighting to circle.", ch);
  }

  if (vict == ch) {
    send_to_char("How can you sneak up on yourself?\r\n", ch);
    return;
  }
  if (!GET_EQ(ch, WEAR_WIELD)) {
    send_to_char("You need to wield a weapon to make it a success.\r\n", ch);
    return;
  }
  if (GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 3) != TYPE_PIERCE - TYPE_HIT) {
    send_to_char("Only piercing weapons can be used for circling.\r\n", ch);
    return;
  }

  if (MOB_FLAGGED(vict, MOB_AWARE) && AWAKE(vict)) {
    act("You notice $N lunging at you!", FALSE, vict, 0, ch, TO_CHAR);
    act("$e notices you lunging at $m!", FALSE, vict, 0, ch, TO_VICT);
    act("$n notices $N lunging at $m!", FALSE, vict, 0, ch, TO_NOTVICT);
    hit(vict, ch, TYPE_UNDEFINED);
    return;
  }

  percent = number(1, 101);	/* 101% is a complete failure */
  prob = GET_SKILL(ch, SKILL_CIRCLE);

  if (AWAKE(vict) && (percent > prob))
    damage(ch, vict, 0, SKILL_CIRCLE);
  else  	
    hit(ch, vict, SKILL_CIRCLE);

  if(!IS_NPC(ch))
	improve_skill(ch, SKILL_CIRCLE);
  WAIT_STATE(ch, 2 * PULSE_VIOLENCE);
}

ACMD(do_betray)
{
  struct char_data *vict, *tmp_ch;
  struct follow_type *f;
  int percent, prob, found = 0;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_RESCUE)) {
    send_to_char("You have no idea how to do that.\r\n", ch);
    return;
  }

  one_argument(argument, arg);

  if (!(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
    send_to_char("Whom do you want to betray?\r\n", ch);
    return;
  }
  if (vict == ch) {
    send_to_char("What about fleeing instead?\r\n", ch);
    return;
  }
  if (FIGHTING(ch) == vict) {
    send_to_char("How can you betray someone you are trying to kill?\r\n", ch);
    return;
  }
  tmp_ch = FIGHTING(ch);

  for (f = ch->followers; f; f = f->next)
    if (AFF_FLAGGED(f->follower, AFF_GROUP) && f->follower->in_room == ch->in_room && f->follower == vict && FIGHTING(vict) == tmp_ch)
		found = 1;

  if (found == 0) {
    act("But $M is not fighting in your group!", FALSE, ch, 0, vict, TO_CHAR);
    return;
  }
  percent = number(1, 101);	/* 101% is a complete failure */
  prob = GET_SKILL(ch, SKILL_RESCUE);

  if (percent > prob) {
    send_to_char("You fail to betray!\r\n", ch);
    return;
  }
  send_to_char("Banzai!  To the rescue...\r\n", ch);
  act("You are betrayed by $N, you are confused!", FALSE, vict, 0, ch, TO_CHAR);
  act("$n sadisticly betrays $N!", FALSE, ch, 0, vict, TO_NOTVICT);

  if (FIGHTING(vict) == tmp_ch)
    stop_fighting(vict);
  if (FIGHTING(tmp_ch))
    stop_fighting(tmp_ch);
  if (FIGHTING(ch))
    stop_fighting(ch);

  set_fighting(vict, tmp_ch);
  set_fighting(tmp_ch, vict);
  if(!IS_NPC(ch))
	improve_skill(ch, SKILL_BETRAY);
  WAIT_STATE(ch, 2 * PULSE_VIOLENCE);
}