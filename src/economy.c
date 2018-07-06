/*************************************************************************
 * The Mythran Mud Economy Snippet Version 3 - updated for CircleMUD use!
 *
 * Copyrights and rules for using the economy system:
 *
 *      The Mythran Mud Economy system was written by The Maniac, it was
 *      loosly based on the rather simple 'Ack!'s banking system'
 *
 *      If you use this code you must follow these rules.
 *              -Keep all the credits in the code.
 *              -Mail Maniac (v942346@si.hhs.nl) to say you use the code
 *              -Send a bug report, if you find 'it'
 *              -Credit me somewhere in your mud.
 *              -Follow the envy/merc/diku license
 *              -If you want to: send me some of your code
 *
 * All my snippets can be found on http://www.hhs.nl/~v942346/snippets.html
 * Check it often because it's growing rapidly  -- Maniac --
 *
 **************************************************************************
 *
 * Version 3 represents a port to CircleMUD done by Edward Glamkowski.
 * Earlier version were written for use with Envy MUD - please contact
 * the Maniac with any questions concerning Envy MUD or the use of this
 * code in that MUD base.  If you are using CircleMUD and have any 
 * comments, questions or concerns, feel free to contact me at:
 * eglamkowski@angelfire.com
 *
 * Please follow the above usage rules, and please also include me in
 * your credits! =)
 *
 **************************************************************************/

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "interpreter.h"
#include "comm.h"
#include "db.h"
#include "handler.h"
#include "economy.h"

int share_value = SHARE_VALUE;      /* External share_value by Maniac */

/* Local functions */
ACMD(do_bank);
ACMD(do_update);
void bank_update(void);
void load_bank(void);

/* Extern variables */
extern struct room_data *world;
extern struct time_info_data time_info;

extern struct char_data *get_char_vis(struct char_data * ch, char *name, 
				      int where);


SPECIAL(bank_teller)
{
  /* The Mythran mud economy system (bank and trading)
   *
   * based on:
   * Simple banking system. by -- Stephen --
   *
   * The following changes and additions where
   * made by the Maniac from Mythran Mud
   * (v942346@si.hhs.nl)
   *
   * History:
   * 18/05/96:     Added the transfer option, enables chars to transfer
   *               money from their account to other players' accounts
   * 18/05/96:     Big bug detected, can deposit/withdraw/transfer
   *               negative amounts (nice way to steal is
   *               bank transfer -(lots of dogh)
   *               Fixed it (thought this was better... -= Maniac =-)
   * 21/06/96:     Fixed a bug in transfer (transfer to MOBS)
   *               Moved balance from ch->balance to ch->pcdata->balance
   * 21/06/96:     Started on the invest option, so players can invest
   *               money in shares, using buy, sell and check
   *               Finished version 1.0 releasing it monday 24/06/96
   * 24/06/96:     Mythran Mud Economy System V1.0 released by Maniac
   *
   *********************************************************************
   * Further changes by Edward Glamkowski (eglamkowski@angelfire.com):
   *
   * 09/05/99:     
   * 1. Ported to circle mud.
   *               
   * 2. Moved all bank code to this file (bank_update, load_bank) and created
   *    an economy.h with detailed notes on relevent variables.  Both invest
   *    and transfer options work, though the invest code is boring (need to
   *    tie it in to the amount of business the shops do!) and the transfer
   *    option is a really bad hack, code-wise.
   *
   * 3. Converted to be a SPECIAL() instead of an ACMD().  Lots of clean-up
   *    to account for this.  Be sure to assign it to a mob to make use of it 
   *    (an example mob, #2, is provided).  Note that this proc can safely
   *    co-exist with the obj bank proc.
   *
   * 4. No longer uses do_say(), but send_to_char() in all cases.
   *
   * 5. Added a 'shares' option, to query how many shares you have, but it
   *    does not tell you about net worth.
   *
   * 6. Rewrote load_bank() to report errors and recover from them.
   */
  struct char_data *mob = (struct char_data *)me;
  int amount = 0;
  char say_buf[MAX_STRING_LENGTH];

#ifdef BANK_TRANSFER
  struct char_data *victim = NULL;
  char buf3[MAX_STRING_LENGTH];
#endif

  if (cmd != find_command("bank"))
    /* Pass control back to the main interpreter -ejg */
    return (FALSE);


  if (mob == NULL) {
    /* Should never happen, but just in case... -ejg */
    send_to_char("You can't do that here.\r\n", ch);
    log("SYSERR: NULL me pointer in bank_teller()!");
    return (FALSE);
  }


  if (IS_NPC(ch)) {
    send_to_char("Banking Services are only available to players!\r\n", ch);
    return (TRUE);
  }


  if (!argument || !*argument) {
    send_to_char("Bank Options:\r\n\r\n", ch);
    send_to_char("Bank balance  : Displays your balance.\r\n", ch);
    send_to_char("Bank deposit  : Deposit gold into your account.\r\n", ch);
    send_to_char("Bank withdraw : Withdraw gold from your account.\r\n", ch);
#ifdef BANK_TRANSFER
    send_to_char("Bank transfer : Transfer gold to another player's account.\r\n", ch);
#endif
#ifdef BANK_INVEST
    send_to_char("Bank buy #    : Buy # shares\r\n", ch);
    send_to_char("Bank sell #   : Sell # shares\r\n", ch);
    send_to_char("Bank shares   : Check how many shares you own\r\n", ch);
    send_to_char("Bank check    : Check the current rates of the shares.\r\n", ch);
#endif
    return (TRUE);
  }

  *say_buf = '\0';

  /* Yuck - this is a horrible hack, but we may need a third argument
   * if we are doing a transfer option... -ejg */
  half_chop(argument, buf1, arg);
  half_chop(arg, buf2, argument);


  /* Now work out what to do... */
  if (!str_cmp(buf1, "balance")) {
    sprintf(say_buf, "%s tells you, 'Your current balance is: "
	    "%d gold.'\r\n", CAP(GET_NAME(mob)), GET_BALANCE(ch));
    send_to_char(say_buf, ch);
    return (TRUE);
  }

  if (!str_cmp(buf1, "deposit")) {
    if (is_number (buf2)) {
      amount = atoi(buf2);

      if (amount > GET_GOLD(ch)) {
	sprintf(say_buf, "%s asks you, 'How can you deposit %d gold "
		"when you only have %d?'\r\n", CAP(GET_NAME(mob)), amount, 
		GET_GOLD(ch));
	send_to_char(say_buf, ch);
	return (TRUE);
      }

      GET_GOLD(ch) -= amount;
      GET_BALANCE(ch) += amount;
      sprintf(say_buf, "%s tells you, 'You deposit %d gold.  Your "
	      "new balance is %d gold.'\r\n", CAP(GET_NAME(mob)), amount, 
	      GET_BALANCE(ch));
      send_to_char(say_buf, ch);
      return (TRUE);
    }
  }
    
#if defined BANK_TRANSFER
  if (!str_cmp(buf1, "transfer")) {
    half_chop(argument, buf3, arg);

    victim = get_char_vis(ch, buf3, FIND_CHAR_WORLD);
    /* Blah - needs to be able to transfer to anybody, including
     * players you can't see or players not currently logged on.  
     * -ejg */

    if (victim == NULL) {
      sprintf(say_buf, "%s asks you, 'Transfer gold to WHOSE account?!'\r\n",
	      CAP(GET_NAME(mob)));
      send_to_char(say_buf, ch);
      return (TRUE);
    }

    if (IS_NPC(victim)) {
      sprintf(say_buf, "%s tells you, 'You can't transfer gold to a mob!\r\n",
	      CAP(GET_NAME(mob)));
      send_to_char(say_buf, ch);
      return (TRUE);
    }

    if (is_number(buf2)) {
      amount = atoi(buf2);

      if (amount > GET_BALANCE(ch)) {
	sprintf(say_buf, "%s asks you, 'How can you transfer %d "
		"gold when your balance is %d?'\r\n", CAP(GET_NAME(mob)), 
		amount, GET_BALANCE(ch));
	send_to_char(say_buf, ch);
	return (TRUE);
      }

      GET_BALANCE(ch) -= amount;
      GET_BALANCE(victim) += amount;
      sprintf(say_buf, "%s tells you, 'You transfer %d gold to %s. "
	      "Your new balance is %d.'\r\n", CAP(GET_NAME(mob)), amount,
	      GET_NAME(victim), GET_BALANCE(ch));
      send_to_char(say_buf, ch);

      sprintf(say_buf, "[BANK] %s has transferred %d gold to your "
	      "account.\r\n",  GET_NAME(ch), amount);
      send_to_char(say_buf, victim);
      return (TRUE);
    }
  }
#endif

  if (!str_cmp(buf1, "withdraw")) {
    if (is_number(buf2)) {
      amount = atoi(buf2);

      if (amount > GET_BALANCE(ch)) {
	sprintf(say_buf, "%s asks you, 'How can you withdraw %d "
		"gold when your balance is %d?'\r\n", CAP(GET_NAME(mob)),
		amount, GET_BALANCE(ch));
	send_to_char(say_buf, ch);
	return (TRUE);
      }

      GET_BALANCE(ch) -= amount;
      GET_GOLD(ch) += amount;
      sprintf(say_buf, "%s tells you, 'You withdraw %d gold.  Your "
	      "new balance is %d gold.'\r\n", CAP(GET_NAME(mob)), amount,
	      GET_BALANCE(ch));
      send_to_char(say_buf, ch);
      return (TRUE);
    }
  }

#if defined BANK_INVEST
  if (!str_cmp(buf1, "buy")) {
    if (is_number(buf2)) {
      amount = atoi(buf2);

      if ((amount * share_value) > GET_BALANCE(ch)) {
	sprintf(say_buf, "%s tells you, '%d shares will cost you "
		"%d gold, get more money!'\r\n", CAP(GET_NAME(mob)), amount,
		(amount * share_value));
	send_to_char(say_buf, ch);
	return (TRUE);
      }

      GET_BALANCE(ch) -= (amount * share_value);
      GET_SHARES(ch) += amount;
      sprintf(say_buf, "%s tells you, 'You buy %d shares for %d "
	      "gold, you now have %d shares.'\r\n", CAP(GET_NAME(mob)), amount,
	      (amount * share_value), GET_SHARES(ch));
      send_to_char(say_buf, ch);
      return (TRUE);
    }
  }

  if (!str_cmp(buf1, "sell")) {
    if (is_number (buf2)) {
      amount = atoi(buf2);

      if (amount > GET_SHARES(ch)) {
	sprintf(say_buf, "%s tells you, 'You only have %d "
		"shares!'\r\n", CAP(GET_NAME(mob)), GET_SHARES(ch));
	send_to_char(say_buf, ch);
	return (TRUE);
      }

      GET_BALANCE(ch) += (amount * share_value);
      GET_SHARES(ch) -= amount;
      sprintf(say_buf, "%s informs you, 'You sell %d shares for %d "
	      "gold, you now have %d shares.\r\n",  CAP(GET_NAME(mob)), amount,
	      (amount * share_value), GET_SHARES(ch));
      send_to_char(say_buf, ch);
      return (TRUE);
    }
  }

  if (!str_cmp(buf1, "shares")) {
    sprintf(say_buf, "%s informs you, 'You have %d shares.'\r\n", 
	    CAP(GET_NAME(mob)), GET_SHARES(ch));
    send_to_char(say_buf, ch);
    return (TRUE);
  }

  if (!str_cmp(buf1, "check")) {
    sprintf(say_buf, "%s informs you, 'The current shareprice is "
	    "%d.'\r\n", CAP(GET_NAME(mob)), share_value);
    send_to_char(say_buf, ch);

    if (GET_SHARES(ch)) {
      sprintf(say_buf, "%s informs you further, 'You currently "
	      "have %d shares.\r\nAt %d a share, your net worth is %d "
	      "gold.'\r\n", CAP(GET_NAME(mob)), GET_SHARES(ch), share_value, 
	      (GET_SHARES(ch) * share_value));
      send_to_char(say_buf, ch);
    }
    return (TRUE);
  }
#endif

  return (FALSE);
}


ACMD(do_update)
{
  one_argument(argument, arg);

  if (!arg || !*arg) {
    send_to_char("Current options to update: \r\n\r\n", ch);
    send_to_char("bank: Update the stock market ticker.\r\n", ch);
    return;
  }
  
  if (!str_cmp(arg, "bank")) {
	sprintf(buf, "Bank update by: %s",GET_NAME(ch));
    mudlog(buf, CMP, LVL_GOD, FALSE);
    bank_update();
    send_to_char("Ok...bank updated.\r\n", ch);
  }
  /* Add new update options here */

  return;
}

		  
/*
 * Update the bank system
 * (C) 1996 The Maniac from Mythran Mud
 *
 * This updates the shares value (I hope)
 *
 * (It does, but not in an interesting way!  
 *  To the IMP reading this: change it! -ejg)
 */
void bank_update(void)
{
  int   value = 0;
  FILE *fp;

  if ((time_info.hours > 45))
    return;         /* Bank is closed, no market... */
  
  value = number(0, 200);
  value -= 100;
  value /= 10;
  
  share_value += value;
  
  if (!(fp = fopen(BANK_FILE, "w"))) {
    log("bank_update:  Couldn't open file %s", BANK_FILE);
    return;
  }

  fprintf(fp, "SHARE_VALUE %d\r\n", share_value);
  fclose(fp);

  return;
}


/*
 * Load the bank information (economy info)
 * By Maniac from Mythran Mud
 *
 * Totally re-written by Ed Glamkowski, now includes error checking
 * and recovery.
 */
void load_bank(void)
{
  FILE *fp;
  char word[256];
  int  number = 0;

  if (!(fp = fopen(BANK_FILE, "r")))
    return;

  if (fscanf(fp, "%s %d", word, &number) != 2) {
    log("SYSERR:  Failed to load bank, using default value");
    number = SHARE_VALUE;
  }

  if (word == NULL || str_cmp(word, "SHARE_VALUE")) {
    log("SYSERR:  Error in bank file (encountered %s).  Using default value",
	word);
    number = SHARE_VALUE;
  }

  share_value = number;

  return;
}
