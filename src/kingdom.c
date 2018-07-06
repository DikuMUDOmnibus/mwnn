/* kingdom.c
Everything to do with kingdoms and hometowns :)
Brian*/

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "interpreter.h"
#include "spells.h"
#include "handler.h"
#include "comm.h"
#include "db.h"
#include "royal.h"


extern struct descriptor_data *descriptor_list;

int num_of_kings = NUM_KINGS;
/*char *ntitle_male(int rank){
  switch (rank) {
      case 8: return "King";
	  case 7: return "Prince";
	  case 6: return "Duke";
	  case 5: return "Marquess";
	  case 4: return "Earl";
      case 3: return "Viscount";
      case 2: return "Baron";
      default: return "Lord";
	  }
}

char *ntitle_female(int rank){
  switch (rank) {
      case 8: return "Queen";
	  case 7: return "Princess";
	  case 6: return "Duchess";
	  case 5: return "Marchioness";
	  case 4: return "Countess";
      case 3: return "Viscountess";
      case 2: return "Baroness";
      default: return "Lady";
	  }
}*/

const char *ntitle_female[] = {
"Peasant",
"Knight",	
"Lady",
"Baroness",
"Viscountess",
"Countess",
"Marchioness",
"Duchess",
"Princess",
"Queen",
"\n"
};

const char *ntitle_male[] = {
"Peasant",
"Knight",
"Lord",
"Baron",
"Viscount",
"Earl",
"Marquess",
"Duke",
"Prince",
"King",
"\n"
};

const char *kingdom_names[] = {
  "Midgaard",
  "\n"
};

void save_kings()
{
FILE *fl;

if (!(fl = fopen(KINGS_FILE, "wb"))) {
  log("SYSERR: Unable to open kingdom file");
  return; }

fwrite(&num_of_kings, sizeof(int), 1, fl);
fwrite(kings, sizeof(struct kingdom_rec), NUM_KINGS, fl);
fclose(fl);
return;
}


void init_kings()
{
FILE *fl;
int i, j;
extern int top_of_p_table;
extern struct player_index_element *player_table;
struct char_file_u chdata;

memset(kings,0,sizeof(struct kingdom_rec)*50);
i=0;

if (!(fl = fopen(KINGS_FILE, "rb"))) {
  log("   Kingdom file does not exist. Will create a new one");
  save_kings();
  return; }

fread(&num_of_kings, sizeof(int), 1, fl);
fread(kings, sizeof(struct kingdom_rec), NUM_KINGS, fl);
fclose(fl);

log("   Calculating population of Kingdoms");
for(i=0;i<NUM_KINGS;i++) {
  kings[i].population = 0;
  
}

for (j = 0; j <= top_of_p_table; j++){
  load_char((player_table + j)->name, &chdata);
  if((i = chdata.kingdom) >= 0) {
    kings[i].population++;
  }
}

return;
}

ACMD(do_tax){
	int tax;

	one_argument(argument, arg);

	if(IS_NPC(ch)){
		send_to_char("Yeah sure whatever..\r\n", ch);
		return;
	}

	if(GET_NOBLE(ch) != 9){
		send_to_char("But you are not King...\r\n", ch);
		return;
	}
	
	if(!*arg){
		sprintf(buf,"Taxes are at %d gold.\r\n",kings[GET_KINGDOM(ch)].taxes);
		send_to_char(buf,ch);
		return;
	}

	tax = atoi(arg);

	kings[GET_KINGDOM(ch)].taxes = tax;

	sprintf(buf,"Taxes have been set to %d.\r\n",tax);
	send_to_char(buf, ch);
	save_kings();
}

ACMD(do_treasure){
	int amount;
	char choice[MAX_INPUT_LENGTH];
	char arg1[MAX_INPUT_LENGTH];

	two_arguments(argument, choice, arg1);

	if(GET_NOBLE(ch) != 9){
		send_to_char("But you are not King...\r\n", ch);
		return;
	}

	if(!*choice){
		sprintf(buf, "You have %d in your treasury.\r\n", kings[GET_KINGDOM(ch)].treasure);
		send_to_char(buf, ch);
		return;
	}

	if(!*arg1){
		send_to_char("How much do you wish to withdraw or deposit?\r\n",ch);
		return;
	}

	amount=atoi(arg1);

	
	if(!strcmp(choice, "withdraw")){
		if(kings[GET_KINGDOM(ch)].treasure < amount){
			send_to_char("You don't have enough in your treasury, sire.\r\n",ch);
			return;
		} else {
			GET_GOLD(ch) += amount;
			kings[GET_KINGDOM(ch)].treasure -=amount;
			sprintf(buf,"You now have %d gold in your treasury, sire.\r\n",kings[GET_KINGDOM(ch)].treasure);
			send_to_char(buf,ch);
			save_kings();
		}
	}else if(!strcmp(choice, "deposit")){
		if(amount > GET_GOLD(ch)){
			send_to_char("You don't have that much gold in your treasury, sire.\r\n",ch);
			return;
		} else {
			GET_GOLD(ch) -= amount;
			kings[GET_KINGDOM(ch)].treasure += amount;
			sprintf(buf,"You now have %d gold in your treasury, sire.\r\n",kings[GET_KINGDOM(ch)].treasure);
			send_to_char(buf,ch);
			save_kings();
		}
	} else {
		send_to_char("Do you wish to deposit or withdraw, sire?\r\n",ch);
		return;
	}
	save_kings();
	
}
		
ACMD(do_war){
	int who;

	one_argument(argument, arg);

	if(GET_NOBLE(ch) != 9){
		send_to_char("You are not king.\r\n",ch);
		return;
	}

	if(!*arg){
		send_to_char("What kingdom do you wish to declare war with, sire?\r\n",ch);
		return;
	}

	who = parse_kingdom(*arg);

	if(who == -1){
		send_to_char("Not a valid Kingdom, Sorry!\r\n",ch);
		return;
	}

	kings[GET_KINGDOM(ch)].war[who] = 1;

	sprintf(buf, "You declare war on ");
	sprinttype(who,kingdom_names,buf2);
	strcat(buf, buf2);

	send_to_char(buf,ch);
	save_kings();
}

ACMD(do_levy)
{
  struct descriptor_data *pt;

  if(GET_NOBLE(ch) != 9)
	  return;

    sprintf(buf, "The King levys taxes of %i.\r\n", kings[GET_KINGDOM(ch)].taxes);
    for (pt = descriptor_list; pt; pt = pt->next)
		if (STATE(pt) == CON_PLAYING && pt->character && pt->character != ch && GET_KINGDOM(pt->character) == GET_KINGDOM(ch)){
			send_to_char(buf, pt->character);
			GET_TAXES(pt->character) += kings[GET_KINGDOM(ch)].taxes;
		}
}

ACMD(do_population)
{
  struct descriptor_data *pt;
  int a = 0;

  if(GET_NOBLE(ch) != 9)
	  return;

  for (pt = descriptor_list; pt; pt = pt->next)
		if (STATE(pt) == CON_PLAYING && pt->character && GET_KINGDOM(pt->character) == GET_KINGDOM(ch))
			a++;

sprintf(buf, "%d of %d of your population are logged on, sire.", a,kings[GET_KINGDOM(ch)].population);
send_to_char(buf,ch);
}

const char *kingdom_abbrevs[] = {
  "Nk",
  "Mi",
  "\n"
};


const char *kingdom[] = {
  "No King",
  "Midgaard",
  "\n"
};


/* The menu for choosing a class in interpreter.c: */
const char *kingdom_menu =
"\r\n"
"Select a kingdom:\r\n"
"  [M]idgaard\r\n";



/*
 * The code to interpret a kingdom letter -- used in interpreter.c when a
 * new character is selecting a kingdom and by 'set kingdom' in act.wizard.c.
 */

int parse_kingdom(char arg)
{
  arg = LOWER(arg);

  switch (arg) {
  case 'm': return KING_MID;
  default:  return KING_NONE;
  }
}

/*
 * bitvectors (i.e., powers of two) for each kingdom, mainly for use in
 * do_who and do_users.  Add new classes at the end so that all classes
 * use sequential powers of two (1 << 0, 1 << 1, 1 << 2, 1 << 3, 1 << 4,
 * 1 << 5, etc.   THEN AGIAN MAYBE NOT
 */

long find_kingdom_bitvector(char arg)
{
  arg = LOWER(arg);

  switch (arg) {
  case 'm': return (1 << KING_MID);
  default:  return 0;
  }
}



