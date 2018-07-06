#include "conf.h"
#include "sysdep.h"


#include "structs.h"
#include "utils.h"
#include "interpreter.h"
#include "spells.h"
#include "handler.h"
#include "comm.h"
#include "db.h"

ACMD(do_challenge)
{
struct char_data *vict;

one_argument(argument, arg);

if(GET_GLADIATOR(ch) == 0){
	send_to_char("You must be a gladiator to do that.\r\n", ch);
	return;
}
else if (!(vict = get_player_vis(ch, arg, FIND_CHAR_WORLD))){
	send_to_char("It is best to challenge someone who is actually here.\r\n", ch);
	return;
}
else if (ch == vict){
    send_to_char("You can not challenge one self.\r\n", ch);
  return;
}
else if (!vict){
	send_to_char("Whom do you wish to challenge?\r\n", ch);
	return;
}
else if(GET_GLADIATOR(vict) == 0){
	send_to_char("Your victim must be a gladiator.\r\n", ch);
	return;
}
else if (!IS_NPC(vict) && !vict->desc){        /* linkless */
    act("$E's linkless at the moment.", FALSE, ch, 0, vict, TO_CHAR | TO_SLEEP);
	return;
}
else if (PLR_FLAGGED(vict, PLR_WRITING)){
	act("$E's writing a message right now; try again later.", FALSE, ch, 0, vict, TO_CHAR | TO_SLEEP);
	return;
}
else if (GET_CHALLENGE(ch) == 1){
	sprintf(buf, "You are being challenged by %s. NACCEPT or NDECLINE first!\r\n", GET_CHALLENGER(ch));
	send_to_char(buf, ch);
	return;
}
else if (GET_CHALLENGE(ch) == 2){
	sprintf(buf, "You are already challenging %s, you must wait.\r\n", GET_CHALLENGER(ch));
	send_to_char(buf, ch);
	return;
}
else if (GET_CHALLENGE(ch) == 3){
	sprintf(buf, "You are already dualing %s, you must wait.\r\n", GET_CHALLENGER(ch));
	send_to_char(buf, ch);
	return;
}
else if (GET_CHALLENGE(vict) >= 1){
	send_to_char("You can not challenge some one who already is challenged.\r\n",ch);
	return;
}
else {

	GET_CHALLENGE(ch) = 2;
	GET_CHALLENGE(vict) = 1;
	GET_CHALLENGER(ch) = vict;
	GET_CHALLENGER(vict) = ch;

	sprintf(buf, "You challenge %s to a dual.\r\n", GET_NAME(vict));
	send_to_char(buf, ch);
	sprintf(buf, "%s challenges you to a dual, NACCEPT or NDECLINE?\r\n", GET_NAME(ch));
	send_to_char(buf, vict);
}
}

ACMD(do_accept){
	struct char_data *vict;
	room_vnum vto_room = 1291;
	room_rnum to_room;

	if(GET_CHALLENGE(ch) != 1){
		send_to_char("You are not challenged.", ch);
		return;
	}

	vict = GET_CHALLENGER(ch);

	GET_CHALLENGE(ch) = 3;
	GET_CHALLENGE(vict) = 3;

	sprintf(buf, "You accept %s's challenge and are magically transported into the arena.\r\n", GET_NAME(vict));
	send_to_char(buf, ch);

	vto_room = number(1291, 1299);
	to_room = real_room(vto_room);

	act("$n goes to fight THE DUAL.", TRUE, ch, 0, 0, TO_NOTVICT);
	char_from_room(ch);

	char_to_room(ch, to_room);
	look_at_room(ch, 0);

	sprintf(buf, "%s accepts your challenge and you are transported to the arena.\r\n", GET_NAME(ch));
	send_to_char(buf, vict);
	
	vto_room = number(1291, 1299);
	to_room = real_room(vto_room);

    act("$n goes to fight THE DUAL.", TRUE, vict, 0, 0, TO_NOTVICT);
	char_from_room(vict);

	char_to_room(vict, to_room);
	look_at_room(vict, 0);
}

ACMD(do_decline){
	struct char_data *vict;

	if(GET_CHALLENGE(ch) != 1){
		send_to_char("You are not challenged.", ch);
		return;
	}

	vict = GET_CHALLENGER(ch);

	GET_CHALLENGE(ch) = 0;
	GET_CHALLENGE(vict) = 0;

	sprintf(buf, "You decline %s's challenge and hand your title over.\r\n", GET_NAME(vict));
	send_to_char(buf, ch);

	sprintf(buf, "%s declines your challenge and hands over the title.\r\n", GET_NAME(ch));
	send_to_char(buf, vict);
}

ACMD(do_gladiator){
	if(GET_GLADIATOR(ch) == 0){
		send_to_char("You will now be a gladiator.\r\n", ch);
		GET_GLADIATOR(ch) = 1;
		return;
	}

	else if(GET_GLADIATOR(ch) == 1){
		if(GET_CHALLENGE(ch) != 0){
			send_to_char("You must not be challenged to end you Gladiator status.\r\n", ch);
			return;
		}
		else{
		send_to_char("You will no longer be a gladiator.\r\n(You can only turn this option off till level 20!)\r\n", ch);
	GET_GLADIATOR(ch) = 0;
		}
	}
}
