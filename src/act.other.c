/* ************************************************************************
*   File: act.other.c                                   Part of CircleMUD *
*  Usage: Miscellaneous player-level commands                             *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#define __ACT_OTHER_C__

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "spells.h"
#include "screen.h"
#include "house.h"
#include "constants.h"

/* extern variables */
extern struct room_data *world;
extern struct descriptor_data *descriptor_list;
extern struct spell_info_type spell_info[];
extern struct index_data *mob_index;
extern char *class_abbrevs[];
extern int free_rent;
extern int pt_allowed;
extern int max_filesize;
extern int nameserver_is_slow;
extern int auto_save;
extern int track_through_doors;

/* extern procedures */
void list_skills(struct char_data * ch);
void appear(struct char_data * ch);
void write_aliases(struct char_data *ch);
void perform_immort_vis(struct char_data *ch);
SPECIAL(shop_keeper);
ACMD(do_gen_comm);
void die(struct char_data * ch);
void Crash_rentsave(struct char_data * ch, int cost);
void improve_skill(struct char_data *ch, int skill);
void add_follower(struct char_data *ch, struct char_data *leader);

#define CALL_MOVE_COST(ch) ((GET_MAX_MOVE((ch))/100) * 15)
#define CALL_HAWK 25
#define CALL_MONKEY 26
#define CALL_TIGER 27
#define CALL_BEAR 28

/* local functions */
ACMD(do_quit);
ACMD(do_save);
ACMD(do_not_here);
ACMD(do_sneak);
ACMD(do_hide);
ACMD(do_steal);
ACMD(do_practice);
ACMD(do_visible);
ACMD(do_title);
int perform_group(struct char_data *ch, struct char_data *vict);
void print_group(struct char_data *ch);
ACMD(do_group);
ACMD(do_ungroup);
ACMD(do_report);
ACMD(do_split);
ACMD(do_use);
ACMD(do_wimpy);
ACMD(do_display);
ACMD(do_gen_write);
ACMD(do_gen_tog);
ACMD(do_sac);
ACMD(do_train);
ACMD(do_retreat);

ACMD(do_quit)
{
  struct descriptor_data *d, *next_d;

  if (IS_NPC(ch) || !ch->desc)
    return;

  if (subcmd != SCMD_QUIT && GET_LEVEL(ch) < LVL_IMMORT)
    send_to_char("You have to type quit--no less, to quit!\r\n", ch);
  else if (GET_POS(ch) == POS_FIGHTING)
    send_to_char("No way!  You're fighting for your life!\r\n", ch);
  else if (GET_CHALLENGE(ch) == 3)/*-=BAH=-*/
    send_to_char("Finish your Duel.  Then quit.\r\n", ch);
  else if (GET_CHALLENGE(ch) == 2 || GET_CHALLENGE(ch) == 1){
    send_to_char("You cancel your Duel.\r\n", ch);
    send_to_char("Your Duel has been canceled.\r\n", GET_CHALLENGER(ch));
    GET_CHALLENGE(GET_CHALLENGER(ch)) = 0;
    GET_CHALLENGE(ch) = 0;
	send_to_char("please type quit agian.\r\n",ch);
  }
  else if (GET_POS(ch) < POS_STUNNED) {
    send_to_char("You die before your time...\r\n", ch);
    die(ch);
  } 
  else {
	int loadroom = ch->in_room;

    if (!GET_INVIS_LEV(ch))
      act("$n has left the game.", TRUE, ch, 0, 0, TO_ROOM);
    sprintf(buf, "%s has quit the game.", GET_NAME(ch));
    mudlog(buf, NRM, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), TRUE);
    send_to_char("Goodbye, friend.. Come back soon!\r\n", ch);

    /*
     * kill off all sockets connected to the same player as the one who is
     * trying to quit.  Helps to maintain sanity as well as prevent duping.
     */
    for (d = descriptor_list; d; d = next_d) {
      next_d = d->next;
      if (d == ch->desc)
        continue;
      if (d->character && (GET_IDNUM(d->character) == GET_IDNUM(ch)))
        STATE(d) = CON_DISCONNECT;
    }

   if (free_rent)
     Crash_rentsave(ch, 0);

    extract_char(ch);		/* Char is saved in extract char */

    /* If someone is quitting in their house, let them load back here */
    if (ROOM_FLAGGED(loadroom, ROOM_HOUSE))
      save_char(ch, loadroom);
  }
}



ACMD(do_save)
{
  if (IS_NPC(ch) || !ch->desc)
    return;

  /* Only tell the char we're saving if they actually typed "save" */
  if (cmd) {
    /*
     * This prevents item duplication by two PC's using coordinated saves
     * (or one PC with a house) and system crashes. Note that houses are
     * still automatically saved without this enabled. This code assumes
     * that guest immortals aren't trustworthy. If you've disabled guest
     * immortal advances from mortality, you may want < instead of <=.
     */
    if (auto_save && GET_LEVEL(ch) <= LVL_IMMORT) {
      send_to_char("Saving aliases.\r\n", ch);
      write_aliases(ch);
      return;
    }
    sprintf(buf, "Saving %s and aliases.\r\n", GET_NAME(ch));
    send_to_char(buf, ch);
  }

  write_aliases(ch);
  save_char(ch, NOWHERE);
  Crash_crashsave(ch);
  if (ROOM_FLAGGED(ch->in_room, ROOM_HOUSE_CRASH))
    House_crashsave(GET_ROOM_VNUM(IN_ROOM(ch)));
}


/* generic function for commands which are normally overridden by
   special procedures - i.e., shop commands, mail commands, etc. */
ACMD(do_not_here)
{
  send_to_char("Sorry, but you cannot do that here!\r\n", ch);
}



ACMD(do_sneak)
{
  struct affected_type af;
  byte percent;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_SNEAK)) {
    send_to_char("You have no idea how to do that.\r\n", ch);
    return;
  }
  send_to_char("Okay, you'll try to move silently for a while.\r\n", ch);
  if (AFF_FLAGGED(ch, AFF_SNEAK))
    affect_from_char(ch, SKILL_SNEAK);

  percent = number(1, 101);	/* 101% is a complete failure */

  if (percent > GET_SKILL(ch, SKILL_SNEAK) + dex_app_skill[GET_DEX(ch)].sneak)
    return;
  
  if(!IS_NPC(ch))
	improve_skill(ch, SKILL_SNEAK);
  af.type = SKILL_SNEAK;
  af.duration = GET_SKILL(ch, SKILL_SNEAK);
  af.modifier = 0;
  af.location = APPLY_NONE;
  af.bitvector = AFF_SNEAK;
  affect_to_char(ch, &af);
}



ACMD(do_hide)
{
  byte percent;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_HIDE)) {
    send_to_char("You have no idea how to do that.\r\n", ch);
    return;
  }

  send_to_char("You attempt to hide yourself.\r\n", ch);

  if (AFF_FLAGGED(ch, AFF_HIDE))
    REMOVE_BIT(AFF_FLAGS(ch), AFF_HIDE);

  percent = number(1, 101);	/* 101% is a complete failure */

  if (percent > GET_SKILL(ch, SKILL_HIDE) + dex_app_skill[GET_DEX(ch)].hide)
    return;
  if(!IS_NPC(ch))
	improve_skill(ch, SKILL_HIDE);
  SET_BIT(AFF_FLAGS(ch), AFF_HIDE);
}




ACMD(do_steal)
{
  struct char_data *vict;
  struct obj_data *obj;
  char vict_name[MAX_INPUT_LENGTH], obj_name[MAX_INPUT_LENGTH];
  int percent, gold, eq_pos, pcsteal = 0, ohoh = 0;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_STEAL)) {
    send_to_char("You have no idea how to do that.\r\n", ch);
    return;
  }
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char("This room just has such a peaceful, easy feeling...\r\n", ch);
    return;
  }

  two_arguments(argument, obj_name, vict_name);

  if (!(vict = get_char_vis(ch, vict_name, FIND_CHAR_ROOM))) {
    send_to_char("Steal what from who?\r\n", ch);
    return;
  } else if (vict == ch) {
    send_to_char("Come on now, that's rather stupid!\r\n", ch);
    return;
  }

  /* 101% is a complete failure */
  percent = number(1, 101) - dex_app_skill[GET_DEX(ch)].p_pocket;

  if (GET_POS(vict) < POS_SLEEPING)
    percent = -1;		/* ALWAYS SUCCESS, unless heavy object. */
	
  if (!pt_allowed && !IS_NPC(vict))
    pcsteal = 1;

  if (!AWAKE(vict))	/* Easier to steal from sleeping people. */
    percent -= 50;

  /* NO NO With Imp's and Shopkeepers, and if player thieving is not allowed */
  if (GET_LEVEL(vict) > LVL_IMMORT || pcsteal ||
      GET_MOB_SPEC(vict) == shop_keeper)
    percent = 101;		/* Failure */

  if (str_cmp(obj_name, "coins") && str_cmp(obj_name, "gold")) {

    if (!(obj = get_obj_in_list_vis(ch, obj_name, vict->carrying))) {

      for (eq_pos = 0; eq_pos < NUM_WEARS; eq_pos++)
	if (GET_EQ(vict, eq_pos) &&
	    (isname(obj_name, GET_EQ(vict, eq_pos)->name)) &&
	    CAN_SEE_OBJ(ch, GET_EQ(vict, eq_pos))) {
	  obj = GET_EQ(vict, eq_pos);
	  break;
	}
      if (!obj) {
	act("$E hasn't got that item.", FALSE, ch, 0, vict, TO_CHAR);
	return;
      } else {			/* It is equipment */
	if ((GET_POS(vict) > POS_STUNNED)) {
	  send_to_char("Steal the equipment now?  Impossible!\r\n", ch);
	  return;
	} else {
	  act("You unequip $p and steal it.", FALSE, ch, obj, 0, TO_CHAR);
	  act("$n steals $p from $N.", FALSE, ch, obj, vict, TO_NOTVICT);
	  obj_to_char(unequip_char(vict, eq_pos), ch);
	  if(!IS_NPC(ch))
	  	improve_skill(ch, SKILL_STEAL);
	}
      }
    } else {			/* obj found in inventory */

      percent += GET_OBJ_WEIGHT(obj);	/* Make heavy harder */

      if (percent > GET_SKILL(ch, SKILL_STEAL)) {
	ohoh = TRUE;
	send_to_char("Oops..\r\n", ch);
	act("$n tried to steal something from you!", FALSE, ch, 0, vict, TO_VICT);
	act("$n tries to steal something from $N.", TRUE, ch, 0, vict, TO_NOTVICT);
      } else {			/* Steal the item */
	if (IS_CARRYING_N(ch) + 1 < CAN_CARRY_N(ch)) {
	  if (IS_CARRYING_W(ch) + GET_OBJ_WEIGHT(obj) < CAN_CARRY_W(ch)) {
	    obj_from_char(obj);
	    obj_to_char(obj, ch);
	    send_to_char("Got it!\r\n", ch);
		if(!IS_NPC(ch))
			improve_skill(ch, SKILL_STEAL);
	  }
	} else
	  send_to_char("You cannot carry that much.\r\n", ch);
      }
    }
  } else {			/* Steal some coins */
    if (AWAKE(vict) && (percent > GET_SKILL(ch, SKILL_STEAL))) {
      ohoh = TRUE;
      send_to_char("Oops..\r\n", ch);
      act("You discover that $n has $s hands in your wallet.", FALSE, ch, 0, vict, TO_VICT);
      act("$n tries to steal gold from $N.", TRUE, ch, 0, vict, TO_NOTVICT);
    } else {
      /* Steal some gold coins */
      gold = (int) ((GET_GOLD(vict) * number(1, 10)) / 100);
      gold = MIN(1782, gold);
      if (gold > 0) {
	GET_GOLD(ch) += gold;
	GET_GOLD(vict) -= gold;
        if (gold > 1) {
	  sprintf(buf, "Bingo!  You got %d gold coins.\r\n", gold);
	  send_to_char(buf, ch);
	} else {
	  send_to_char("You manage to swipe a solitary gold coin.\r\n", ch);
	}
	if(!IS_NPC(ch))
		improve_skill(ch, SKILL_STEAL);
      } else {
	send_to_char("You couldn't get any gold...\r\n", ch);
      }
    }
  }

  if (ohoh && IS_NPC(vict) && AWAKE(vict))
    hit(vict, ch, TYPE_UNDEFINED);
}



ACMD(do_practice)
{
  if (IS_NPC(ch))
    return;

  one_argument(argument, arg);

  if (*arg)
    send_to_char("You can only practice skills in your guild.\r\n", ch);
  else
    list_skills(ch);
}



ACMD(do_visible)
{
  if (GET_LEVEL(ch) >= LVL_IMMORT) {
    perform_immort_vis(ch);
    return;
  }

  if AFF_FLAGGED(ch, AFF_INVISIBLE) {
    appear(ch);
    send_to_char("You break the spell of invisibility.\r\n", ch);
  } else
    send_to_char("You are already visible.\r\n", ch);
}



ACMD(do_title)
{
  skip_spaces(&argument);
  delete_doubledollar(argument);

  if (IS_NPC(ch))
    send_to_char("Your title is fine... go away.\r\n", ch);
  else if (PLR_FLAGGED(ch, PLR_NOTITLE))
    send_to_char("You can't title yourself -- you shouldn't have abused it!\r\n", ch);
  else if (strstr(argument, "(") || strstr(argument, ")"))
    send_to_char("Titles can't contain the ( or ) characters.\r\n", ch);
  else if (strlen(argument) > MAX_TITLE_LENGTH) {
    sprintf(buf, "Sorry, titles can't be longer than %d characters.\r\n",
	    MAX_TITLE_LENGTH);
    send_to_char(buf, ch);
  } else {
    set_title(ch, argument);
    sprintf(buf, "Okay, you're now %s %s.\r\n", GET_NAME(ch), GET_TITLE(ch));
    send_to_char(buf, ch);
  }
}


int perform_group(struct char_data *ch, struct char_data *vict)
{
  if (AFF_FLAGGED(vict, AFF_GROUP) || !CAN_SEE(ch, vict))
    return (0);

  SET_BIT(AFF_FLAGS(vict), AFF_GROUP);
  if (ch != vict)
    act("$N is now a member of your group.", FALSE, ch, 0, vict, TO_CHAR);
  act("You are now a member of $n's group.", FALSE, ch, 0, vict, TO_VICT);
  act("$N is now a member of $n's group.", FALSE, ch, 0, vict, TO_NOTVICT);
  return (1);
}


void print_group(struct char_data *ch)
{
  struct char_data *k;
  struct follow_type *f;

  if (!AFF_FLAGGED(ch, AFF_GROUP))
    send_to_char("But you are not the member of a group!\r\n", ch);
  else {
    send_to_char("Your group consists of:\r\n", ch);

    k = (ch->master ? ch->master : ch);

    if (AFF_FLAGGED(k, AFF_GROUP)) {
      sprintf(buf, "     [%3dH %3dM %3dV] [%2d %s] $N (Head of group)",
	      GET_HIT(k), GET_MANA(k), GET_MOVE(k), GET_LEVEL(k), CLASS_ABBR(k));
      act(buf, FALSE, ch, 0, k, TO_CHAR);
    }

    for (f = k->followers; f; f = f->next) {
      if (!AFF_FLAGGED(f->follower, AFF_GROUP))
	continue;

      sprintf(buf, "     [%3dH %3dM %3dV] [%2d %s] $N", GET_HIT(f->follower),
	      GET_MANA(f->follower), GET_MOVE(f->follower),
	      GET_LEVEL(f->follower), CLASS_ABBR(f->follower));
      act(buf, FALSE, ch, 0, f->follower, TO_CHAR);
    }
  }
}



ACMD(do_group)
{
  struct char_data *vict;
  struct follow_type *f;
  int found;

  one_argument(argument, buf);

  if (!*buf) {
    print_group(ch);
    return;
  }

  if (ch->master) {
    act("You can not enroll group members without being head of a group.",
	FALSE, ch, 0, 0, TO_CHAR);
    return;
  }

  if (!str_cmp(buf, "all")) {
    perform_group(ch, ch);
    for (found = 0, f = ch->followers; f; f = f->next)
      found += perform_group(ch, f->follower);
    if (!found)
      send_to_char("Everyone following you is already in your group.\r\n", ch);
    return;
  }

  if (!(vict = get_char_vis(ch, buf, FIND_CHAR_ROOM)))
    send_to_char(NOPERSON, ch);
  else if ((vict->master != ch) && (vict != ch))
    act("$N must follow you to enter your group.", FALSE, ch, 0, vict, TO_CHAR);
  else {
    if (!AFF_FLAGGED(vict, AFF_GROUP))
      perform_group(ch, vict);
    else {
      if (ch != vict)
	act("$N is no longer a member of your group.", FALSE, ch, 0, vict, TO_CHAR);
      act("You have been kicked out of $n's group!", FALSE, ch, 0, vict, TO_VICT);
      act("$N has been kicked out of $n's group!", FALSE, ch, 0, vict, TO_NOTVICT);
      REMOVE_BIT(AFF_FLAGS(vict), AFF_GROUP);
    }
  }
}



ACMD(do_ungroup)
{
  struct follow_type *f, *next_fol;
  struct char_data *tch;

  one_argument(argument, buf);

  if (!*buf) {
    if (ch->master || !(AFF_FLAGGED(ch, AFF_GROUP))) {
      send_to_char("But you lead no group!\r\n", ch);
      return;
    }
    sprintf(buf2, "%s has disbanded the group.\r\n", GET_NAME(ch));
    for (f = ch->followers; f; f = next_fol) {
      next_fol = f->next;
      if (AFF_FLAGGED(f->follower, AFF_GROUP)) {
	REMOVE_BIT(AFF_FLAGS(f->follower), AFF_GROUP);
	send_to_char(buf2, f->follower);
        if (!AFF_FLAGGED(f->follower, AFF_CHARM))
	  stop_follower(f->follower);
      }
    }

    REMOVE_BIT(AFF_FLAGS(ch), AFF_GROUP);
    send_to_char("You disband the group.\r\n", ch);
    return;
  }
  if (!(tch = get_char_vis(ch, buf, FIND_CHAR_ROOM))) {
    send_to_char("There is no such person!\r\n", ch);
    return;
  }
  if (tch->master != ch) {
    send_to_char("That person is not following you!\r\n", ch);
    return;
  }

  if (!AFF_FLAGGED(tch, AFF_GROUP)) {
    send_to_char("That person isn't in your group.\r\n", ch);
    return;
  }

  REMOVE_BIT(AFF_FLAGS(tch), AFF_GROUP);

  act("$N is no longer a member of your group.", FALSE, ch, 0, tch, TO_CHAR);
  act("You have been kicked out of $n's group!", FALSE, ch, 0, tch, TO_VICT);
  act("$N has been kicked out of $n's group!", FALSE, ch, 0, tch, TO_NOTVICT);
 
  if (!AFF_FLAGGED(tch, AFF_CHARM))
    stop_follower(tch);
}




ACMD(do_report)
{
  struct char_data *k;
  struct follow_type *f;

  if (!AFF_FLAGGED(ch, AFF_GROUP)) {
    send_to_char("But you are not a member of any group!\r\n", ch);
    return;
  }
  sprintf(buf, "%s reports: %d/%dH, %d/%dM, %d/%dV\r\n",
	  GET_NAME(ch), GET_HIT(ch), GET_MAX_HIT(ch),
	  GET_MANA(ch), GET_MAX_MANA(ch),
	  GET_MOVE(ch), GET_MAX_MOVE(ch));

  CAP(buf);

  k = (ch->master ? ch->master : ch);

  for (f = k->followers; f; f = f->next)
    if (AFF_FLAGGED(f->follower, AFF_GROUP) && f->follower != ch)
      send_to_char(buf, f->follower);
  if (k != ch)
    send_to_char(buf, k);
  send_to_char("You report to the group.\r\n", ch);
}



ACMD(do_split)
{
  int amount, num, share, rest;
  struct char_data *k;
  struct follow_type *f;

  if (IS_NPC(ch))
    return;

  one_argument(argument, buf);

  if (is_number(buf)) {
    amount = atoi(buf);
    if (amount <= 0) {
      send_to_char("Sorry, you can't do that.\r\n", ch);
      return;
    }
    if (amount > GET_GOLD(ch)) {
      send_to_char("You don't seem to have that much gold to split.\r\n", ch);
      return;
    }
    k = (ch->master ? ch->master : ch);

    if (AFF_FLAGGED(k, AFF_GROUP) && (k->in_room == ch->in_room))
      num = 1;
    else
      num = 0;

    for (f = k->followers; f; f = f->next)
      if (AFF_FLAGGED(f->follower, AFF_GROUP) &&
	  (!IS_NPC(f->follower)) &&
	  (f->follower->in_room == ch->in_room))
	num++;

    if (num && AFF_FLAGGED(ch, AFF_GROUP)) {
      share = amount / num;
      rest = amount % num;
    } else {
      send_to_char("With whom do you wish to share your gold?\r\n", ch);
      return;
    }

    GET_GOLD(ch) -= share * (num - 1);

    sprintf(buf, "%s splits %d coins; you receive %d.\r\n", GET_NAME(ch),
            amount, share);
    if (rest) {
      sprintf(buf + strlen(buf), "%d coin%s %s not splitable, so %s "
              "keeps the money.\r\n", rest,
              (rest == 1) ? "" : "s",
              (rest == 1) ? "was" : "were",
              GET_NAME(ch));
    }
    if (AFF_FLAGGED(k, AFF_GROUP) && (k->in_room == ch->in_room)
	&& !(IS_NPC(k)) && k != ch) {
      GET_GOLD(k) += share;
      send_to_char(buf, k);
    }
    for (f = k->followers; f; f = f->next) {
      if (AFF_FLAGGED(f->follower, AFF_GROUP) &&
	  (!IS_NPC(f->follower)) &&
	  (f->follower->in_room == ch->in_room) &&
	  f->follower != ch) {
	GET_GOLD(f->follower) += share;
	send_to_char(buf, f->follower);
      }
    }
    sprintf(buf, "You split %d coins among %d members -- %d coins each.\r\n",
	    amount, num, share);
    if (rest) {
      sprintf(buf + strlen(buf), "%d coin%s %s not splitable, so you keep "
                                 "the money.\r\n", rest,
                                 (rest == 1) ? "" : "s",
                                 (rest == 1) ? "was" : "were");
      GET_GOLD(ch) += rest;
    }
    send_to_char(buf, ch);
  } else {
    send_to_char("How many coins do you wish to split with your group?\r\n", ch);
    return;
  }
}



ACMD(do_use)
{
  struct obj_data *mag_item;

  half_chop(argument, arg, buf);
  if (!*arg) {
    sprintf(buf2, "What do you want to %s?\r\n", CMD_NAME);
    send_to_char(buf2, ch);
    return;
  }
  mag_item = GET_EQ(ch, WEAR_HOLD);

  if (!mag_item || !isname(arg, mag_item->name)) {
    switch (subcmd) {
    case SCMD_RECITE:
    case SCMD_QUAFF:
      if (!(mag_item = get_obj_in_list_vis(ch, arg, ch->carrying))) {
	sprintf(buf2, "You don't seem to have %s %s.\r\n", AN(arg), arg);
	send_to_char(buf2, ch);
	return;
      }
      break;
    case SCMD_USE:
      sprintf(buf2, "You don't seem to be holding %s %s.\r\n", AN(arg), arg);
      send_to_char(buf2, ch);
      return;
    default:
      log("SYSERR: Unknown subcmd %d passed to do_use.", subcmd);
      return;
    }
  }
  switch (subcmd) {
  case SCMD_QUAFF:
    if (GET_OBJ_TYPE(mag_item) != ITEM_POTION) {
      send_to_char("You can only quaff potions.\r\n", ch);
      return;
    }
    break;
  case SCMD_RECITE:
    if (GET_OBJ_TYPE(mag_item) != ITEM_SCROLL) {
      send_to_char("You can only recite scrolls.\r\n", ch);
      return;
    }
    break;
  case SCMD_USE:
    if ((GET_OBJ_TYPE(mag_item) != ITEM_WAND) &&
	(GET_OBJ_TYPE(mag_item) != ITEM_STAFF)) {
      send_to_char("You can't seem to figure out how to use it.\r\n", ch);
      return;
    }
    break;
  }

  mag_objectmagic(ch, mag_item, buf);
}



ACMD(do_wimpy)
{
  int wimp_lev;

  /* 'wimp_level' is a player_special. -gg 2/25/98 */
  if (IS_NPC(ch))
    return;

  one_argument(argument, arg);

  if (!*arg) {
    if (GET_WIMP_LEV(ch)) {
      sprintf(buf, "Your current wimp level is %d hit points.\r\n",
	      GET_WIMP_LEV(ch));
      send_to_char(buf, ch);
      return;
    } else {
      send_to_char("At the moment, you're not a wimp.  (sure, sure...)\r\n", ch);
      return;
    }
  }
  if (isdigit(*arg)) {
    if ((wimp_lev = atoi(arg)) != 0) {
      if (wimp_lev < 0)
	send_to_char("Heh, heh, heh.. we are jolly funny today, eh?\r\n", ch);
      else if (wimp_lev > GET_MAX_HIT(ch))
	send_to_char("That doesn't make much sense, now does it?\r\n", ch);
      else if (wimp_lev > (GET_MAX_HIT(ch) / 2))
	send_to_char("You can't set your wimp level above half your hit points.\r\n", ch);
      else {
	sprintf(buf, "Okay, you'll wimp out if you drop below %d hit points.\r\n",
		wimp_lev);
	send_to_char(buf, ch);
	GET_WIMP_LEV(ch) = wimp_lev;
      }
    } else {
      send_to_char("Okay, you'll now tough out fights to the bitter end.\r\n", ch);
      GET_WIMP_LEV(ch) = 0;
    }
  } else
    send_to_char("Specify at how many hit points you want to wimp out at.  (0 to disable)\r\n", ch);
}


ACMD(do_display)
{
  size_t i;

  if (IS_NPC(ch)) {
    send_to_char("Mosters don't need displays.  Go away.\r\n", ch);
    return;
  }
  skip_spaces(&argument);

  if (!*argument) {
    send_to_char("Usage: prompt { { H | M | V } | all | none }\r\n", ch);
    return;
  }
  if (!str_cmp(argument, "on") || !str_cmp(argument, "all"))
    SET_BIT(PRF_FLAGS(ch), PRF_DISPHP | PRF_DISPMANA | PRF_DISPMOVE);
  else if (!str_cmp(argument, "off") || !str_cmp(argument, "none"))
    REMOVE_BIT(PRF_FLAGS(ch), PRF_DISPHP | PRF_DISPMANA | PRF_DISPMOVE);
  else {
    REMOVE_BIT(PRF_FLAGS(ch), PRF_DISPHP | PRF_DISPMANA | PRF_DISPMOVE);

    for (i = 0; i < strlen(argument); i++) {
      switch (LOWER(argument[i])) {
      case 'h':
	SET_BIT(PRF_FLAGS(ch), PRF_DISPHP);
	break;
      case 'm':
	SET_BIT(PRF_FLAGS(ch), PRF_DISPMANA);
	break;
      case 'v':
	SET_BIT(PRF_FLAGS(ch), PRF_DISPMOVE);
	break;
      default:
	send_to_char("Usage: prompt { { H | M | V } | all | none }\r\n", ch);
	return;
      }
    }
  }

  send_to_char(OK, ch);
}



ACMD(do_gen_write)
{
  FILE *fl;
  char *tmp, buf[MAX_STRING_LENGTH];
  const char *filename;
  struct stat fbuf;
  time_t ct;

  switch (subcmd) {
  case SCMD_BUG:
    filename = BUG_FILE;
    break;
  case SCMD_TYPO:
    filename = TYPO_FILE;
    break;
  case SCMD_IDEA:
    filename = IDEA_FILE;
    break;
  default:
    return;
  }

  ct = time(0);
  tmp = asctime(localtime(&ct));

  if (IS_NPC(ch)) {
    send_to_char("Monsters can't have ideas - Go away.\r\n", ch);
    return;
  }

  skip_spaces(&argument);
  delete_doubledollar(argument);

  if (!*argument) {
    send_to_char("That must be a mistake...\r\n", ch);
    return;
  }
  sprintf(buf, "%s %s: %s", GET_NAME(ch), CMD_NAME, argument);
  mudlog(buf, CMP, LVL_IMMORT, FALSE);

  if (stat(filename, &fbuf) < 0) {
    perror("SYSERR: Can't stat() file");
    return;
  }
  if (fbuf.st_size >= max_filesize) {
    send_to_char("Sorry, the file is full right now.. try again later.\r\n", ch);
    return;
  }
  if (!(fl = fopen(filename, "a"))) {
    perror("SYSERR: do_gen_write");
    send_to_char("Could not open the file.  Sorry.\r\n", ch);
    return;
  }
  fprintf(fl, "%-8s (%6.6s) [%5d] %s\n", GET_NAME(ch), (tmp + 4),
	  GET_ROOM_VNUM(IN_ROOM(ch)), argument);
  fclose(fl);
  send_to_char("Okay.  Thanks!\r\n", ch);
}



#define TOG_OFF 0
#define TOG_ON  1

#define PRF_TOG_CHK(ch,flag) ((TOGGLE_BIT(PRF_FLAGS(ch), (flag))) & (flag))

ACMD(do_gen_tog)
{
  long result;

  const char *tog_messages[][2] = {
    {"You are now safe from summoning by other players.\r\n",
    "You may now be summoned by other players.\r\n"},
    {"Nohassle disabled.\r\n",
    "Nohassle enabled.\r\n"},
    {"Brief mode off.\r\n",
    "Brief mode on.\r\n"},
    {"Compact mode off.\r\n",
    "Compact mode on.\r\n"},
    {"You can now hear tells.\r\n",
    "You are now deaf to tells.\r\n"},
    {"You can now hear auctions.\r\n",
    "You are now deaf to auctions.\r\n"},
    {"You can now hear shouts.\r\n",
    "You are now deaf to shouts.\r\n"},
    {"You can now hear OOC.\r\n",
    "You are now deaf to OOC.\r\n"},
    {"You can now hear the congratulation messages.\r\n",
    "You are now deaf to the congratulation messages.\r\n"},
    {"You can now hear the Wiz-channel.\r\n",
    "You are now deaf to the Wiz-channel.\r\n"},
    {"You are no longer part of the Quest.\r\n",
    "Okay, you are part of the Quest!\r\n"},
    {"You will no longer see the room flags.\r\n",
    "You will now see the room flags.\r\n"},
    {"You will now have your communication repeated.\r\n",
    "You will no longer have your communication repeated.\r\n"},
    {"HolyLight mode off.\r\n",
    "HolyLight mode on.\r\n"},
    {"Nameserver_is_slow changed to NO; IP addresses will now be resolved.\r\n",
    "Nameserver_is_slow changed to YES; sitenames will no longer be resolved.\r\n"},
    {"Autoexits disabled.\r\n",
    "Autoexits enabled.\r\n"},
    {"Will no longer track through doors.\r\n",
    "Will now track through doors.\r\n"},
	{"You will no longer Auto-Assist.\r\n",
    "You will now Auto-Assist.\r\n"},
	{"AutoSplit disabled.\r\n",
    "AutoSplit enabled.\r\n"},
	{"AutoLooting disabled.\r\n",
    "AutoLooting enabled.\r\n"}
  };


  if (IS_NPC(ch))
    return;

  switch (subcmd) {
  case SCMD_NOSUMMON:
    result = PRF_TOG_CHK(ch, PRF_SUMMONABLE);
    break;
  case SCMD_NOHASSLE:
    result = PRF_TOG_CHK(ch, PRF_NOHASSLE);
    break;
  case SCMD_BRIEF:
    result = PRF_TOG_CHK(ch, PRF_BRIEF);
    break;
  case SCMD_COMPACT:
    result = PRF_TOG_CHK(ch, PRF_COMPACT);
    break;
  case SCMD_NOTELL:
    result = PRF_TOG_CHK(ch, PRF_NOTELL);
    break;
  case SCMD_NOAUCTION:
    result = PRF_TOG_CHK(ch, PRF_NOAUCT);
    break;
  case SCMD_DEAF:
    result = PRF_TOG_CHK(ch, PRF_DEAF);
    break;
  case SCMD_NOOOC:
    result = PRF_TOG_CHK(ch, PRF_NOOOC);
    break;
  case SCMD_NOGRATZ:
    result = PRF_TOG_CHK(ch, PRF_NOGRATZ);
    break;
  case SCMD_NOWIZ:
    result = PRF_TOG_CHK(ch, PRF_NOWIZ);
    break;
  case SCMD_QUEST:
    result = PRF_TOG_CHK(ch, PRF_QUEST);
    break;
  case SCMD_ROOMFLAGS:
    result = PRF_TOG_CHK(ch, PRF_ROOMFLAGS);
    break;
  case SCMD_NOREPEAT:
    result = PRF_TOG_CHK(ch, PRF_NOREPEAT);
    break;
  case SCMD_HOLYLIGHT:
    result = PRF_TOG_CHK(ch, PRF_HOLYLIGHT);
    break;
  case SCMD_SLOWNS:
    result = (nameserver_is_slow = !nameserver_is_slow);
    break;
  case SCMD_AUTOEXIT:
    result = PRF_TOG_CHK(ch, PRF_AUTOEXIT);
    break;
  case SCMD_TRACK:
    result = (track_through_doors = !track_through_doors);
    break;
  case SCMD_AUTOASSIST:
    result = PRF_TOG_CHK(ch, PRF_AUTOASSIST);
    break;
  case SCMD_AUTOSPLIT:
    result = PRF_TOG_CHK(ch, PRF_AUTOSPLIT);
    break;
  case SCMD_AUTOLOOT:
    result = PRF_TOG_CHK(ch, PRF_AUTOLOOT);
    break;
  default:
    log("SYSERR: Unknown subcmd %d in do_gen_toggle.", subcmd);
    return;
  }

  if (result)
    send_to_char(tog_messages[subcmd][TOG_ON], ch);
  else
    send_to_char(tog_messages[subcmd][TOG_OFF], ch);

  return;
}

ACMD(do_sac)
{
   struct obj_data *obj;
 
   one_argument(argument, arg);

   // note, I like to take care of no arg and wrong args up front, not
   // at the end of a function, lets get the wrongness out of the way :) 
   if (!*arg)
   {
     send_to_char("You wanted to sacrifice what? \n\r",ch);
     return;
   }

   // if it's not in the room, we ain't gonna sac it
   if (!(obj = get_obj_in_list_vis(ch, arg, world[ch->in_room].contents)))
   {
     send_to_char("That object is not here. \n\r",ch);
     return;
   }

   // nifty, got the object in the room, now check its flags
   if (!CAN_WEAR(obj, ITEM_WEAR_TAKE))
   {
     send_to_char("The would not be an appropriate sacrifice.\n\r",ch);
     return;
   }

   // seems as if everything checks out eh? ok now do it
   act("$n sacrifices $p.", FALSE, ch, obj, 0, TO_ROOM);
   act("You sacrafice $p.\r\nYou have been rewarded.",
	FALSE, ch, obj, 0, TO_CHAR); 
   if (GET_EXP(ch) <= 17500){
	   GET_GOLD(ch) += (GET_OBJ_COST(obj)/4);
   } else if (GET_EXP(ch) <= 40000){
	   GET_S_EXP(ch) += GET_OBJ_COST(obj);
	   GET_EXP(ch) += GET_OBJ_COST(obj);
   } else {
	   GET_RPP(ch) += GET_OBJ_COST(obj);
   }
   
   extract_obj(obj);
}

ACMD(do_train)
{
  one_argument(argument, arg);

  if (*arg)
    send_to_char("You can only train in your guild.\r\n", ch);
  else
  sprintf(buf,
	  "STR: %d  CON: %d  DEX: %d  CHA: %d  WIS: %d  INT: %d\r\n",
	  GET_STR(ch),GET_CON(ch),GET_DEX(ch),GET_CHA(ch), GET_WIS(ch), GET_INT(ch));
 send_to_char(buf, ch);
}

ACMD(do_retreat)
{
 struct char_data *was_fighting;
 extern const char *dirs[];
 int prob, percent, dir = 0;
 int retreat_type;

   one_argument(argument, arg);

 if (!FIGHTING(ch))
   {
   send_to_char("You are not fighting!", ch);
   return;
   }
 
 if (!*arg)
   {
   send_to_char("Retreat where?!?", ch);
   return;
   }


 switch(*arg)
 {
	 *arg = LOWER(*arg);

 case 'n':
	 retreat_type=NORTH;
	 break;
 case 's':
	 retreat_type=SOUTH;
	 break;
 case 'e':
	 retreat_type=EAST;
	 break;
 case 'w':
	 retreat_type=WEST;
	 break;
 case 'u':
	 retreat_type=UP;
	 break;
case 'd':
	 retreat_type=DOWN;
	 break;
default:
	retreat_type=-1;
 }

 if (retreat_type < 0 || !EXIT(ch, retreat_type) ||
   EXIT(ch, retreat_type)->to_room == NOWHERE)
   {
   send_to_char("Retreat where?\r\n", ch);
   return;
   }

 dir = retreat_type;
 percent = GET_SKILL(ch, SKILL_RETREAT);
 prob = number(0, 101);

 if (prob <= percent){
if (CAN_GO(ch, dir) && !ROOM_FLAGGED(EXIT(ch, dir)->to_room, ROOM_DEATH))
  {
       act("$n skillfully retreats from combat.", TRUE, ch, 0, 0,
TO_ROOM);
  send_to_char("You skillfully retreat from combat.\r\n", ch);
  WAIT_STATE(ch, PULSE_VIOLENCE);
  do_simple_move(ch, dir, TRUE);
  was_fighting=FIGHTING(ch);
  stop_fighting(ch);
  if(!IS_NPC(ch))
	improve_skill(ch, SKILL_RETREAT);
  } else {
  act("$n tries to retreat from combat but has no where to go!", TRUE,
ch,
     0, 0, TO_ROOM);
  send_to_char("You cannot retreat in that direction!", ch);
       return;
  }
 } else {
    send_to_char("You fail your attempt to retreat!\r\n", ch);
    WAIT_STATE(ch, PULSE_VIOLENCE);
    return;
  }
}


ACMD(do_pull)
{

  one_argument(argument, arg);

  if (!*arg) {
    send_to_char("Pull what?\r\n", ch);
    return;
  }
/* for now only pull pin will work and must be wielding a grenade */
  if(!str_cmp(arg,"pin")) {
    if(GET_EQ(ch, WEAR_HOLD)) {
      if(GET_OBJ_TYPE(GET_EQ(ch, WEAR_HOLD)) == ITEM_GRENADE) {
        send_to_char("You pull the pin, the grenade is activated!\r\n", ch);
        SET_BIT(GET_OBJ_EXTRA(GET_EQ(ch, WEAR_HOLD)), ITEM_LIVE_GRENADE);
      } else
        send_to_char("That's NOT a grenade!\r\n", ch);
    } else
      send_to_char("You aren't wielding anything!\r\n", ch);
  }

/* put other 'pull' options here later */
 return;
}

ACMD(do_call)
{

  struct char_data *pet;
  struct follow_type *f;
  int chance;
  int penalty=0;

  if (!GET_SKILL(ch, SKILL_CALL_WILD)) {
    send_to_char("You have no idea how.\r\n", ch);
    return;
  }

  if (GET_MOVE(ch) <= CALL_MOVE_COST(ch)) {
    send_to_char("You are too tired.\r\n", ch);
    return;
  }

  GET_MOVE(ch) -= CALL_MOVE_COST(ch);

  for (f = ch->followers; f; f = f->next)
    if (IN_ROOM(ch) == IN_ROOM(f->follower)) {
      if (IS_MOB(f->follower))
        penalty+=5;
      else
        penalty+=2;
    }

  if (GET_CHA(ch) < (penalty + 5)) {
    send_to_char("You can not attract any more followers now.\r\n", ch);
    return;
  }

  chance = number(0, GET_SKILL(ch, SKILL_CALL_WILD));

  if(GET_LEVEL(ch) > LVL_IMMORT + 1)
    chance = number(91,111);
  
  if (chance < 50) {
    act("Your call fails.  No wildlife was attracted.", FALSE, ch,0, 0, TO_CHAR);
    act("$n belts out an impressive call to the wild and looks a bit dissapointed as it remains unanswered.", FALSE, ch, 0, 0, TO_NOTVICT);
    return;
  } else if (chance  < 65)
    pet = read_mobile(CALL_HAWK, VIRTUAL);
  else if (chance  < 75)
    pet = read_mobile(CALL_MONKEY, VIRTUAL);
  else if (chance  < 90)
    pet = read_mobile(CALL_TIGER, VIRTUAL);
  else
    pet = read_mobile(CALL_BEAR, VIRTUAL);

  act("Your call of the wild is answered by $N.", FALSE, ch, 0, pet, TO_CHAR);
  act("$ns call of the wild is answered by $N.", FALSE, ch, 0, pet, TO_ROOM);

  IS_CARRYING_W(pet) = 1000;
  IS_CARRYING_N(pet) = 100;
  SET_BIT(AFF_FLAGS(pet), AFF_CHARM);  

  char_to_room(pet, IN_ROOM(ch));
  add_follower(pet, ch);
}

ACMD(do_fly)
{
	if(GET_RACE(ch) != RACE_FAIRY && GET_RACE(ch) != RACE_ARIEL){
		send_to_char("Huh?!?\r\n", ch);
		return;
	}
	else
		if(!AFF_FLAGGED(ch, AFF_FLY)){
		SET_BIT(AFF_FLAGS(ch), AFF_FLY);
		send_to_char("You rise above the ground.\r\n", ch);
		}
		else
		{
			REMOVE_BIT(AFF_FLAGS(ch), AFF_FLY);
			send_to_char("You slowly float to the ground.\r\n", ch);
		}
}

ACMD(do_spy)
{
   int percent, prob, spy_type, return_room;
   char *spy_dirs[] = {
     "north",
     "east",
     "south",
     "west",
     "up",
     "down",
     "\n"
   };

   /* 101% is a complete failure */
   percent = number(1, 101);
   prob = GET_SKILL(ch, SKILL_SPY);
   spy_type = search_block(argument + 1, spy_dirs, FALSE);

  if (spy_type < 0 || !EXIT(ch, spy_type) || EXIT(ch, spy_type)->to_room == NOWHERE) {
    send_to_char("Spy where?\r\n", ch);
    return;
  }
  else {
     if (!(GET_MOVE(ch) >= 5)) {
        send_to_char("You don't have enough movement points.\r\n", ch);
     }
     else {
        if (percent > prob) {
           send_to_char("You plunder your spy.\r\n", ch);
		   act("$n attempts to peeks into the next room.", TRUE, ch, 0, 0, TO_NOTVICT);
           GET_MOVE(ch) = MAX(0, MIN(GET_MAX_MOVE(ch), GET_MOVE(ch) - 2));
        }
        else {
           if (IS_SET(EXIT(ch, spy_type)->exit_info, EX_CLOSED) && EXIT(ch, spy_type)->keyword) {
              sprintf(buf, "The %s is closed.\r\n", fname(EXIT(ch, spy_type)->keyword));
              send_to_char(buf, ch);
              GET_MOVE(ch) = MAX(0, MIN(GET_MAX_MOVE(ch), GET_MOVE(ch) - 2));
           }
           else {
              GET_MOVE(ch) = MAX(0, MIN(GET_MAX_MOVE(ch), GET_MOVE(ch) - 5));
              return_room = ch->in_room;
              char_from_room(ch);
              char_to_room(ch, world[return_room].dir_option[spy_type]->to_room);
              send_to_char("You spy into the next room and see: \r\n\r\n", ch);
              look_at_room(ch, 1);
              char_from_room(ch);
              char_to_room(ch, return_room);
           }
        }
     }
  }
}


ACMD(do_beast){
	struct affected_type af[MAX_SPELL_AFFECTS];
    bool accum_duration = FALSE;
	int prob, percent;

	if(GET_RACE(ch) != RACE_WERE || GET_BEAST(ch) == BEAST_NONE){
		send_to_char("Huh?!?\r\n",ch);
		return;
	}

	prob = (1, 250);

	percent = GET_SKILL(ch, SKILL_BEAST) + GET_LEVEL(ch);

	if(prob > percent){
		send_to_char("You fail to harness the beast within.\r\n",ch);
		act("$n concentrates and strains attempting to do something.", TRUE, ch, 0, 0, TO_NOTVICT);
		return;
	} else {

		if (AFF_FLAGGED(ch, AFF_BEAST)) {
			prob = (1, 250);
			percent = GET_SKILL(ch, SKILL_BEAST) + GET_LEVEL(ch);
			
			if(prob > percent){
				send_to_char("You can not supress the beast within.\r\n",ch);
				return;
			}

			send_to_char("You supress the beast allowing your human for to take back over.",ch);
			act("$n supresses the beast and slowly returns to human for,.",TRUE,ch,0,0,TO_NOTVICT);
			REMOVE_BIT(AFF_FLAGS(ch), AFF_BEAST);
			
			switch(GET_BEAST(ch)){
				case BEAST_RAT:
					REMOVE_BIT(AFF_FLAGS(ch), AFF_HIDE | AFF_SNEAK | AFF_INFRAVISION);
					break;
				case BEAST_WOLF:
					break;
				case BEAST_BEAR:
					break;
			}

		}else{
		
			af[0].duration = GET_LEVEL(ch) + GET_SKILL(ch, SKILL_BEAST);
			af[0].bitvector = AFF_BEAST;
			
			switch(GET_BEAST(ch)){
				case BEAST_RAT:
					send_to_char("As you concentrate on the beast within you notice\r\n"
						         "changes begin to come over your body.  You begin\r\n"
								 "to shrink in size and miniture claws grow from your\r\n"
								 "hands and feet.  You become a......RAT!\r\n",ch);
					
					af[0].duration = GET_LEVEL(ch) + GET_SKILL(ch, SKILL_BEAST);
					af[0].bitvector = AFF_HIDE;
	
					af[1].duration = GET_LEVEL(ch) + GET_SKILL(ch, SKILL_BEAST);
					af[1].bitvector = AFF_SNEAK;
	
					af[2].duration = GET_LEVEL(ch) + GET_SKILL(ch, SKILL_BEAST);
					af[2].bitvector = AFF_INFRAVISION;
	
					accum_duration = TRUE;
	
					break;
				case BEAST_WOLF:
					send_to_char("As you concentrate on the beast within you notice\r\n"
						         "changes begin to come over your body.  You begin\r\n"
								 "to form paws and claws from your hands and feet.\r\n"
								 "You hunch down on all four as you become...WOLF!\r\n",ch);
					break;
				case BEAST_BEAR:
					send_to_char("As you concentrate on the beast within you notice\r\n"
						         "changes begin to come over your body.  You grow\r\n"
								 "in girth and your skin hardens into an armour. Your\r\n"
								 "body becomes massive as you change into......BEAR!\r\n",ch);
					break;
			}
		}
	}
}