/* ************************************************************************
*   File: class.c                                       Part of CircleMUD *
*  Usage: Source file for class-specific code                             *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

/*
 * This file attempts to concentrate most of the code which must be changed
 * in order for new classes to be added.  If you're adding a new class,
 * you should go through this entire file from beginning to end and add
 * the appropriate new special cases for your new class.
 */



#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "db.h"
#include "utils.h"
#include "spells.h"
#include "interpreter.h"
#include "handler.h"
#include "comm.h"
#include "constants.h"

extern int siteok_everyone;

/* local functions */
int parse_class(char arg);
long find_class_bitvector(char arg);
byte saving_throws(int class_num, int type, int level);
int thaco(int class_num, int level);
void roll_real_abils(struct char_data * ch);
void do_start(struct char_data * ch);
int backstab_mult(int level);
int invalid_class(struct char_data *ch, struct obj_data *obj);
const char *title_male(int chclass, int level);
const char *title_female(int chclass, int level);

/* Names first */

const char *class_abbrevs[] = {
  "Nc",
  "Kn",
  "Th",
  "Me",
  "As",
  "Tr",
  "Ba",
  "Gu",
  "Mt",
  "Li",
  "De",
  "Fi",
  "Wi",
  "Wa",
  "Ea",
  "Il",
  "\n"
};


const char *pc_class_types[] = {
  "No Class",
  "Knight",
  "Thief",
  "Mercenary",
  "Assassin",
  "Tracker",
  "Bard",
  "Merchant",
  "Spy",
  "Guard",
  "Life Mage",
  "Death Mage",
  "Fire Mage",
  "Wind Mage",
  "Water Mage",
  "Earth Mage",
  "Illusionist",
  "Alchemist",
  "\n"
};


/* The menu for choosing a class in interpreter.c: */
const char *class_menu =
"\r\n"
"Select a class:\r\n"
"  1: Knight\r\n"
"  2: Thief\r\n"
"  3: Mercenary\r\n"
"  4: Assassin\r\n"
"  5: Tracker\r\n"
"  6: Bard\r\n"
"  7: Merchant\r\n"
"  8: Spy\r\n"
"  9: Guard\r\n"
"  10: Life Mage\r\n"
"  11: Death Mage\r\n"
"  12: Fire Mage\r\n"
"  13: Wind Mage\r\n"
"  14: Water Mage\r\n"
"  15: Earth Mage\r\n"
"  16: Illusionist\r\n"
"  17: Alchemist\r\n";



/*
 * The code to interpret a class letter -- used in interpreter.c when a
 * new character is selecting a class and by 'set class' in act.wizard.c.
 */

int parse_class(char arg)
{

switch (arg) {
  case '0': return CLASS_NONE;
  case '1': return CLASS_KNIGHT;
  case '2': return CLASS_MERC;
  case '3': return CLASS_ASSASSIN;
  case '4': return CLASS_BARD;
  case '5': return CLASS_THIEF;
  case '6': return CLASS_TRACKER;
  case '7': return CLASS_MERCHANT;
  case '8': return CLASS_SPY;
  case '9': return CLASS_GUARD;
  case '10': return CLASS_LIFE;
  case '11': return CLASS_DEATH;
  case '12': return CLASS_FIRE;
  case '13': return CLASS_WIND;
  case '14': return CLASS_WATER;
  case '15': return CLASS_EARTH;
  case '16': return CLASS_ILLUSION;
  case '17': return CLASS_ALCHEMIST;
  default:  return CLASS_UNDEFINED;
  }
}

/*
 * bitvectors (i.e., powers of two) for each class, mainly for use in
 * do_who and do_users.  Add new classes at the end so that all classes
 * use sequential powers of two (1 << 0, 1 << 1, 1 << 2, 1 << 3, 1 << 4,
 * 1 << 5, etc.
 */

long find_class_bitvector(char arg)
{
  arg = LOWER(arg);

  switch (arg) {
  case 'n': return (1 << CLASS_NONE);
  case 'k': return (1 << CLASS_KNIGHT);
  case 'm': return (1 << CLASS_MERC);
  case 'b': return (1 << CLASS_BARD);
  case 't': return (1 << CLASS_THIEF);
  case 'r': return (1 << CLASS_TRACKER);
  case 'e': return (1 << CLASS_MERCHANT);
  case 's': return (1 << CLASS_SPY);
  case 'g': return (1 << CLASS_GUARD);
  case 'l': return (1 << CLASS_LIFE);
  case 'd': return (1 << CLASS_DEATH);
  case 'f': return (1 << CLASS_FIRE);
  case 'w': return (1 << CLASS_WIND);
  case 'a': return (1 << CLASS_WATER);
  case 'h': return (1 << CLASS_EARTH);
  case 'i': return (1 << CLASS_ILLUSION);
  case 'c': return (1 << CLASS_ALCHEMIST);
  default:  return 0;
  }
}


/*
 * These are definitions which control the guildmasters for each class.
 *
 * The first field (top line) controls the highest percentage skill level
 * a character of the class is allowed to attain in any skill.  (After
 * this level, attempts to practice will say "You are already learned in
 * this area."
 * 
 * The second line controls the maximum percent gain in learnedness a
 * character is allowed per practice -- in other words, if the random
 * die throw comes out higher than this number, the gain will only be
 * this number instead.
 *
 * The third line controls the minimu percent gain in learnedness a
 * character is allowed per practice -- in other words, if the random
 * die throw comes out below this number, the gain will be set up to
 * this number.
 * 
 * The fourth line simply sets whether the character knows 'spells'
 * or 'skills'.  This does not affect anything except the message given
 * to the character when trying to practice (i.e. "You know of the
 * following spells" vs. "You know of the following skills"
 */

#define SPELL	0
#define SKILL	1

/* #define LEARNED_LEVEL	0  % known which is considered "learned" */
/* #define MAX_PER_PRAC		1  max percent gain in skill per practice */
/* #define MIN_PER_PRAC		2  min percent gain in skill per practice */
/* #define PRAC_TYPE		3  should it say 'spell' or 'skill'?	*/

int prac_params[4][NUM_CLASSES] = {
  /* NONE	CLE	    THE  	WAR		ASS		RANG   Mag */
  {75,		90,	    85,	    80,		85,		80,     95},		/* learned level */
  {15,		100,    15,	    25,		20,		30,     100},		/* max per prac */
  {1,		25,	    1,	    1,		1,		1,      25},		/* min per pac */
  {SKILL,	SPELL,	SKILL,	SKILL,	SKILL,	SKILL,  SPELL}		/* prac name */
};


/*
 * ...And the appropriate rooms for each guildmaster/guildguard; controls
 * which types of people the various guildguards let through.  i.e., the
 * first line shows that from room 3017, only MAGIC_USERS are allowed
 * to go south.
 *
 * Don't forget to visit spec_assign.c if you create any new mobiles that
 * should be a guild master or guard so they can act appropriately. If you
 * "recycle" the existing mobs that are used in other guilds for your new
 * guild, then you don't have to change that file, only here.
 */
int guild_info[][3] = {

/* Mencha */
  {CLASS_DEATH,	3017,	SCMD_SOUTH},
  {CLASS_LIFE,	3004,	SCMD_NORTH},
  {CLASS_THIEF,		3027,	SCMD_EAST},
  {CLASS_KNIGHT,	3021,	SCMD_EAST},

/* Brass Dragon */
  {-999 /* all */ ,	5065,	SCMD_WEST},

/* this must go last -- add new guards above! */
  {-1, -1, -1}
};



/*
 * Saving throws for:
 * MCTW
 *   PARA, ROD, PETRI, BREATH, SPELL
 *     Levels 0-40
 *
 * Do not forget to change extern declaration in magic.c if you add to this.
 */

byte saving_throws(int class_num, int type, int level)
{
  switch (class_num) {
  case CLASS_DEATH:
  case CLASS_LIFE:
  case CLASS_FIRE:
  case CLASS_WIND:
  case CLASS_WATER:
  case CLASS_EARTH:
  case CLASS_ILLUSION:
  case CLASS_ALCHEMIST:
    switch (type) {
    case SAVING_PARA:	/* Paralyzation */
      switch (level) {
      case  0: return 90;
      case  1: return 70;
      case  2: return 69;
      case  3: return 68;
      case  4: return 67;
      case  5: return 66;
      case  6: return 65;
      case  7: return 63;
      case  8: return 61;
      case  9: return 60;
      case 10: return 59;
      case 11: return 57;
      case 12: return 55;
      case 13: return 54;
      case 14: return 53;
      case 15: return 53;
      case 16: return 52;
      case 17: return 51;
      case 18: return 50;
      case 19: return 48;
      case 20: return 46;
      case 21: return 45;
      case 22: return 44;
      case 23: return 42;
      case 24: return 40;
      case 25: return 38;
      case 26: return 36;
      case 27: return 34;
      case 28: return 32;
      case 29: return 30;
      case 30: return 28;
      case 31: return  0;
      case 32: return  0;
      case 33: return  0;
      case 34: return  0;
      case 35: return  0;
      case 36: return  0;
      case 37: return  0;
      case 38: return  0;
      case 39: return  0;
      case 40: return  0;
      default:
	log("SYSERR: Missing level for mage paralyzation saving throw.");
	break;
      }
    case SAVING_ROD:	/* Rods */
      switch (level) {
      case  0: return 90;
      case  1: return 55;
      case  2: return 53;
      case  3: return 51;
      case  4: return 49;
      case  5: return 47;
      case  6: return 45;
      case  7: return 43;
      case  8: return 41;
      case  9: return 40;
      case 10: return 39;
      case 11: return 37;
      case 12: return 35;
      case 13: return 33;
      case 14: return 31;
      case 15: return 30;
      case 16: return 29;
      case 17: return 27;
      case 18: return 25;
      case 19: return 23;
      case 20: return 21;
      case 21: return 20;
      case 22: return 19;
      case 23: return 17;
      case 24: return 15;
      case 25: return 14;
      case 26: return 13;
      case 27: return 12;
      case 28: return 11;
      case 29: return 10;
      case 30: return  9;
      case 31: return  0;
      case 32: return  0;
      case 33: return  0;
      case 34: return  0;
      case 35: return  0;
      case 36: return  0;
      case 37: return  0;
      case 38: return  0;
      case 39: return  0;
      case 40: return  0;
      default:
	log("SYSERR: Missing level for mage rod saving throw.");
	break;
      }
    case SAVING_PETRI:	/* Petrification */
      switch (level) {
      case  0: return 90;
      case  1: return 65;
      case  2: return 63;
      case  3: return 61;
      case  4: return 59;
      case  5: return 57;
      case  6: return 55;
      case  7: return 53;
      case  8: return 51;
      case  9: return 50;
      case 10: return 49;
      case 11: return 47;
      case 12: return 45;
      case 13: return 43;
      case 14: return 41;
      case 15: return 40;
      case 16: return 39;
      case 17: return 37;
      case 18: return 35;
      case 19: return 33;
      case 20: return 31;
      case 21: return 30;
      case 22: return 29;
      case 23: return 27;
      case 24: return 25;
      case 25: return 23;
      case 26: return 21;
      case 27: return 19;
      case 28: return 17;
      case 29: return 15;
      case 30: return 13;
      case 31: return  0;
      case 32: return  0;
      case 33: return  0;
      case 34: return  0;
      case 35: return  0;
      case 36: return  0;
      case 37: return  0;
      case 38: return  0;
      case 39: return  0;
      case 40: return  0;
      default:
	log("SYSERR: Missing level for mage petrification saving throw.");
	break;
      }
    case SAVING_BREATH:	/* Breath weapons */
      switch (level) {
      case  0: return 90;
      case  1: return 75;
      case  2: return 73;
      case  3: return 71;
      case  4: return 69;
      case  5: return 67;
      case  6: return 65;
      case  7: return 63;
      case  8: return 61;
      case  9: return 60;
      case 10: return 59;
      case 11: return 57;
      case 12: return 55;
      case 13: return 53;
      case 14: return 51;
      case 15: return 50;
      case 16: return 49;
      case 17: return 47;
      case 18: return 45;
      case 19: return 43;
      case 20: return 41;
      case 21: return 40;
      case 22: return 39;
      case 23: return 37;
      case 24: return 35;
      case 25: return 33;
      case 26: return 31;
      case 27: return 29;
      case 28: return 27;
      case 29: return 25;
      case 30: return 23;
      case 31: return  0;
      case 32: return  0;
      case 33: return  0;
      case 34: return  0;
      case 35: return  0;
      case 36: return  0;
      case 37: return  0;
      case 38: return  0;
      case 39: return  0;
      case 40: return  0;
      default:
	log("SYSERR: Missing level for mage breath saving throw.");
	break;
      }
    case SAVING_SPELL:	/* Generic spells */
      switch (level) {
      case  0: return 90;
      case  1: return 60;
      case  2: return 58;
      case  3: return 56;
      case  4: return 54;
      case  5: return 52;
      case  6: return 50;
      case  7: return 48;
      case  8: return 46;
      case  9: return 45;
      case 10: return 44;
      case 11: return 42;
      case 12: return 40;
      case 13: return 38;
      case 14: return 36;
      case 15: return 35;
      case 16: return 34;
      case 17: return 32;
      case 18: return 30;
      case 19: return 28;
      case 20: return 26;
      case 21: return 25;
      case 22: return 24;
      case 23: return 22;
      case 24: return 20;
      case 25: return 18;
      case 26: return 16;
      case 27: return 14;
      case 28: return 12;
      case 29: return 10;
      case 30: return  8;
      case 31: return  0;
      case 32: return  0;
      case 33: return  0;
      case 34: return  0;
      case 35: return  0;
      case 36: return  0;
      case 37: return  0;
      case 38: return  0;
      case 39: return  0;
      case 40: return  0;
      default:
	log("SYSERR: Missing level for mage spell saving throw.");
	break;
      }
    default:
      log("SYSERR: Invalid saving throw type.");
      break;
    }
    break;
  
  case CLASS_NONE:
  case CLASS_ASSASSIN:
  case CLASS_THIEF:
    switch (type) {
    case SAVING_PARA:	/* Paralyzation */
      switch (level) {
      case  0: return 90;
      case  1: return 65;
      case  2: return 64;
      case  3: return 63;
      case  4: return 62;
      case  5: return 61;
      case  6: return 60;
      case  7: return 59;
      case  8: return 58;
      case  9: return 57;
      case 10: return 56;
      case 11: return 55;
      case 12: return 54;
      case 13: return 53;
      case 14: return 52;
      case 15: return 51;
      case 16: return 50;
      case 17: return 49;
      case 18: return 48;
      case 19: return 47;
      case 20: return 46;
      case 21: return 45;
      case 22: return 44;
      case 23: return 43;
      case 24: return 42;
      case 25: return 41;
      case 26: return 40;
      case 27: return 39;
      case 28: return 38;
      case 29: return 37;
      case 30: return 36;
      case 31: return  0;
      case 32: return  0;
      case 33: return  0;
      case 34: return  0;
      case 35: return  0;
      case 36: return  0;
      case 37: return  0;
      case 38: return  0;
      case 39: return  0;
      case 40: return  0;
      default:
	log("SYSERR: Missing level for thief paralyzation saving throw.");
	break;
      }
    case SAVING_ROD:	/* Rods */
      switch (level) {
      case  0: return 90;
      case  1: return 70;
      case  2: return 68;
      case  3: return 66;
      case  4: return 64;
      case  5: return 62;
      case  6: return 60;
      case  7: return 58;
      case  8: return 56;
      case  9: return 54;
      case 10: return 52;
      case 11: return 50;
      case 12: return 48;
      case 13: return 46;
      case 14: return 44;
      case 15: return 42;
      case 16: return 40;
      case 17: return 38;
      case 18: return 36;
      case 19: return 34;
      case 20: return 32;
      case 21: return 30;
      case 22: return 28;
      case 23: return 26;
      case 24: return 24;
      case 25: return 22;
      case 26: return 20;
      case 27: return 18;
      case 28: return 16;
      case 29: return 14;
      case 30: return 13;
      case 31: return  0;
      case 32: return  0;
      case 33: return  0;
      case 34: return  0;
      case 35: return  0;
      case 36: return  0;
      case 37: return  0;
      case 38: return  0;
      case 39: return  0;
      case 40: return  0;
      default:
	log("SYSERR: Missing level for thief rod saving throw.");
	break;
      }
    case SAVING_PETRI:	/* Petrification */
      switch (level) {
      case  0: return 90;
      case  1: return 60;
      case  2: return 59;
      case  3: return 58;
      case  4: return 58;
      case  5: return 56;
      case  6: return 55;
      case  7: return 54;
      case  8: return 53;
      case  9: return 52;
      case 10: return 51;
      case 11: return 50;
      case 12: return 49;
      case 13: return 48;
      case 14: return 47;
      case 15: return 46;
      case 16: return 45;
      case 17: return 44;
      case 18: return 43;
      case 19: return 42;
      case 20: return 41;
      case 21: return 40;
      case 22: return 39;
      case 23: return 38;
      case 24: return 37;
      case 25: return 36;
      case 26: return 35;
      case 27: return 34;
      case 28: return 33;
      case 29: return 32;
      case 30: return 31;
      case 31: return  0;
      case 32: return  0;
      case 33: return  0;
      case 34: return  0;
      case 35: return  0;
      case 36: return  0;
      case 37: return  0;
      case 38: return  0;
      case 39: return  0;
      case 40: return  0;
      default:
	log("SYSERR: Missing level for thief petrification saving throw.");
	break;
      }
    case SAVING_BREATH:	/* Breath weapons */
      switch (level) {
      case  0: return 90;
      case  1: return 80;
      case  2: return 79;
      case  3: return 78;
      case  4: return 77;
      case  5: return 76;
      case  6: return 75;
      case  7: return 74;
      case  8: return 73;
      case  9: return 72;
      case 10: return 71;
      case 11: return 70;
      case 12: return 69;
      case 13: return 68;
      case 14: return 67;
      case 15: return 66;
      case 16: return 65;
      case 17: return 64;
      case 18: return 63;
      case 19: return 62;
      case 20: return 61;
      case 21: return 60;
      case 22: return 59;
      case 23: return 58;
      case 24: return 57;
      case 25: return 56;
      case 26: return 55;
      case 27: return 54;
      case 28: return 53;
      case 29: return 52;
      case 30: return 51;
      case 31: return  0;
      case 32: return  0;
      case 33: return  0;
      case 34: return  0;
      case 35: return  0;
      case 36: return  0;
      case 37: return  0;
      case 38: return  0;
      case 39: return  0;
      case 40: return  0;
      default:
	log("SYSERR: Missing level for thief breath saving throw.");
	break;
      }
    case SAVING_SPELL:	/* Generic spells */
      switch (level) {
      case  0: return 90;
      case  1: return 75;
      case  2: return 73;
      case  3: return 71;
      case  4: return 69;
      case  5: return 67;
      case  6: return 65;
      case  7: return 63;
      case  8: return 61;
      case  9: return 59;
      case 10: return 57;
      case 11: return 55;
      case 12: return 53;
      case 13: return 51;
      case 14: return 49;
      case 15: return 47;
      case 16: return 45;
      case 17: return 43;
      case 18: return 41;
      case 19: return 39;
      case 20: return 37;
      case 21: return 35;
      case 22: return 33;
      case 23: return 31;
      case 24: return 29;
      case 25: return 27;
      case 26: return 25;
      case 27: return 23;
      case 28: return 21;
      case 29: return 19;
      case 30: return 17;
      case 31: return  0;
      case 32: return  0;
      case 33: return  0;
      case 34: return  0;
      case 35: return  0;
      case 36: return  0;
      case 37: return  0;
      case 38: return  0;
      case 39: return  0;
      case 40: return  0;
      default:
	log("SYSERR: Missing level for thief spell saving throw.");
	break;
      }
    default:
      log("SYSERR: Invalid saving throw type.");
      break;
    }
    break;
  
  case CLASS_SPY:
  case CLASS_GUARD:
  case CLASS_TRACKER:
  case CLASS_KNIGHT:
    switch (type) {
    case SAVING_PARA:	/* Paralyzation */
      switch (level) {
      case  0: return 90;
      case  1: return 70;
      case  2: return 68;
      case  3: return 67;
      case  4: return 65;
      case  5: return 62;
      case  6: return 58;
      case  7: return 55;
      case  8: return 53;
      case  9: return 52;
      case 10: return 50;
      case 11: return 47;
      case 12: return 43;
      case 13: return 40;
      case 14: return 38;
      case 15: return 37;
      case 16: return 35;
      case 17: return 32;
      case 18: return 28;
      case 19: return 25;
      case 20: return 24;
      case 21: return 23;
      case 22: return 22;
      case 23: return 20;
      case 24: return 19;
      case 25: return 17;
      case 26: return 16;
      case 27: return 15;
      case 28: return 14;
      case 29: return 13;
      case 30: return 12;
      case 31: return 11;
      case 32: return 10;
      case 33: return  9;
      case 34: return  8;
      case 35: return  7;
      case 36: return  6;
      case 37: return  5;
      case 38: return  4;
      case 39: return  3;
      case 40: return  2;
      case 41: return  1;	/* Some mobiles. */
      case 42: return  0;
      case 43: return  0;
      case 44: return  0;
      case 45: return  0;
      case 46: return  0;
      case 47: return  0;
      case 48: return  0;
      case 49: return  0;
      case 50: return  0;
      default:
	log("SYSERR: Missing level for knight paralyzation saving throw.");
	break;	
      }
    case SAVING_ROD:	/* Rods */
      switch (level) {
      case  0: return 90;
      case  1: return 80;
      case  2: return 78;
      case  3: return 77;
      case  4: return 75;
      case  5: return 72;
      case  6: return 68;
      case  7: return 65;
      case  8: return 63;
      case  9: return 62;
      case 10: return 60;
      case 11: return 57;
      case 12: return 53;
      case 13: return 50;
      case 14: return 48;
      case 15: return 47;
      case 16: return 45;
      case 17: return 42;
      case 18: return 38;
      case 19: return 35;
      case 20: return 34;
      case 21: return 33;
      case 22: return 32;
      case 23: return 30;
      case 24: return 29;
      case 25: return 27;
      case 26: return 26;
      case 27: return 25;
      case 28: return 24;
      case 29: return 23;
      case 30: return 22;
      case 31: return 20;
      case 32: return 18;
      case 33: return 16;
      case 34: return 14;
      case 35: return 12;
      case 36: return 10;
      case 37: return  8;
      case 38: return  6;
      case 39: return  5;
      case 40: return  4;
      case 41: return  3;
      case 42: return  2;
      case 43: return  1;
      case 44: return  0;
      case 45: return  0;
      case 46: return  0;
      case 47: return  0;
      case 48: return  0;
      case 49: return  0;
      case 50: return  0;
      default:
	log("SYSERR: Missing level for knight rod saving throw.");
	break;
      }
    case SAVING_PETRI:	/* Petrification */
      switch (level) {
      case  0: return 90;
      case  1: return 75;
      case  2: return 73;
      case  3: return 72;
      case  4: return 70;
      case  5: return 67;
      case  6: return 63;
      case  7: return 60;
      case  8: return 58;
      case  9: return 57;
      case 10: return 55;
      case 11: return 52;
      case 12: return 48;
      case 13: return 45;
      case 14: return 43;
      case 15: return 42;
      case 16: return 40;
      case 17: return 37;
      case 18: return 33;
      case 19: return 30;
      case 20: return 29;
      case 21: return 28;
      case 22: return 26;
      case 23: return 25;
      case 24: return 24;
      case 25: return 23;
      case 26: return 21;
      case 27: return 20;
      case 28: return 19;
      case 29: return 18;
      case 30: return 17;
      case 31: return 16;
      case 32: return 15;
      case 33: return 14;
      case 34: return 13;
      case 35: return 12;
      case 36: return 11;
      case 37: return 10;
      case 38: return  9;
      case 39: return  8;
      case 40: return  7;
      case 41: return  6;
      case 42: return  5;
      case 43: return  4;
      case 44: return  3;
      case 45: return  2;
      case 46: return  1;
      case 47: return  0;
      case 48: return  0;
      case 49: return  0;
      case 50: return  0;
      default:
	log("SYSERR: Missing level for knight petrification saving throw.");
	break;
      }
    case SAVING_BREATH:	/* Breath weapons */
      switch (level) {
      case  0: return 90;
      case  1: return 85;
      case  2: return 83;
      case  3: return 82;
      case  4: return 80;
      case  5: return 75;
      case  6: return 70;
      case  7: return 65;
      case  8: return 63;
      case  9: return 62;
      case 10: return 60;
      case 11: return 55;
      case 12: return 50;
      case 13: return 45;
      case 14: return 43;
      case 15: return 42;
      case 16: return 40;
      case 17: return 37;
      case 18: return 33;
      case 19: return 30;
      case 20: return 29;
      case 21: return 28;
      case 22: return 26;
      case 23: return 25;
      case 24: return 24;
      case 25: return 23;
      case 26: return 21;
      case 27: return 20;
      case 28: return 19;
      case 29: return 18;
      case 30: return 17;
      case 31: return 16;
      case 32: return 15;
      case 33: return 14;
      case 34: return 13;
      case 35: return 12;
      case 36: return 11;
      case 37: return 10;
      case 38: return  9;
      case 39: return  8;
      case 40: return  7;
      case 41: return  6;
      case 42: return  5;
      case 43: return  4;
      case 44: return  3;
      case 45: return  2;
      case 46: return  1;
      case 47: return  0;
      case 48: return  0;
      case 49: return  0;
      case 50: return  0;
      default:
	log("SYSERR: Missing level for knight breath saving throw.");
	break;
      }
    case SAVING_SPELL:	/* Generic spells */
      switch (level) {
      case  0: return 90;
      case  1: return 85;
      case  2: return 83;
      case  3: return 82;
      case  4: return 80;
      case  5: return 77;
      case  6: return 73;
      case  7: return 70;
      case  8: return 68;
      case  9: return 67;
      case 10: return 65;
      case 11: return 62;
      case 12: return 58;
      case 13: return 55;
      case 14: return 53;
      case 15: return 52;
      case 16: return 50;
      case 17: return 47;
      case 18: return 43;
      case 19: return 40;
      case 20: return 39;
      case 21: return 38;
      case 22: return 36;
      case 23: return 35;
      case 24: return 34;
      case 25: return 33;
      case 26: return 31;
      case 27: return 30;
      case 28: return 29;
      case 29: return 28;
      case 30: return 27;
      case 31: return 25;
      case 32: return 23;
      case 33: return 21;
      case 34: return 19;
      case 35: return 17;
      case 36: return 15;
      case 37: return 13;
      case 38: return 11;
      case 39: return  9;
      case 40: return  7;
      case 41: return  6;
      case 42: return  5;
      case 43: return  4;
      case 44: return  3;
      case 45: return  2;
      case 46: return  1;
      case 47: return  0;
      case 48: return  0;
      case 49: return  0;
      case 50: return  0;
      default:
	log("SYSERR: Missing level for knight spell saving throw.");
	break;
      }
    default:
      log("SYSERR: Invalid saving throw type.");
      break;
    }
  default:
    log("SYSERR: Invalid class saving throw.");
    break;
  }

  /* Should not get here unless something is wrong. */
  return 100;
}

/* THAC0 for classes and levels.  (To Hit Armor Class 0) */
int thaco(int class_num, int level)
{
  switch (class_num) {
  
  case CLASS_DEATH:
  case CLASS_LIFE:
  case CLASS_WIND:
  case CLASS_WATER:
  case CLASS_FIRE:
  case CLASS_EARTH:
  case CLASS_ILLUSION:
  case CLASS_ALCHEMIST:
    switch (level) {
    case  0: return 100;
    case  1: return  20;
    case  2: return  20;
    case  3: return  20;
    case  4: return  19;
    case  5: return  19;
    case  6: return  19;
    case  7: return  18;
    case  8: return  18;
    case  9: return  18;
    case 10: return  17;
    case 11: return  17;
    case 12: return  17;
    case 13: return  16;
    case 14: return  16;
    case 15: return  16;
    case 16: return  15;
    case 17: return  15;
    case 18: return  15;
    case 19: return  14;
    case 20: return  14;
    case 21: return  14;
    case 22: return  13;
    case 23: return  13;
    case 24: return  13;
    case 25: return  12;
    case 26: return  12;
    case 27: return  12;
    case 28: return  11;
    case 29: return  11;
    case 30: return  11;
    case 31: return  10;
    case 32: return  10;
    case 33: return  10;
    case 34: return   9;
    default:
      log("SYSERR: Missing level for mage thac0.");
    }
	
	case CLASS_TRACKER:
	case CLASS_NONE:
	case CLASS_GUARD:
    switch (level) {
    case  0: return 100;
    case  1: return  20;
    case  2: return  20;
    case  3: return  20;
    case  4: return  18;
    case  5: return  18;
    case  6: return  18;
    case  7: return  16;
    case  8: return  16;
    case  9: return  16;
    case 10: return  14;
    case 11: return  14;
    case 12: return  14;
    case 13: return  12;
    case 14: return  12;
    case 15: return  12;
    case 16: return  10;
    case 17: return  10;
    case 18: return  10;
    case 19: return   8;
    case 20: return   8;
    case 21: return   8;
    case 22: return   6;
    case 23: return   6;
    case 24: return   6;
    case 25: return   4;
    case 26: return   4;
    case 27: return   4;
    case 28: return   2;
    case 29: return   2;
    case 30: return   2;
    case 31: return   1;
    case 32: return   1;
    case 33: return   1;
    case 34: return   1;
    default:
      log("SYSERR: Missing level for tracker, guard, none thac0.");
    }
  case CLASS_THIEF:
  case CLASS_SPY:
    switch (level) {
    case  0: return 100;
    case  1: return  20;
    case  2: return  20;
    case  3: return  19;
    case  4: return  19;
    case  5: return  18;
    case  6: return  18;
    case  7: return  17;
    case  8: return  17;
    case  9: return  16;
    case 10: return  16;
    case 11: return  15;
    case 12: return  15;
    case 13: return  14;
    case 14: return  14;
    case 15: return  13;
    case 16: return  13;
    case 17: return  12;
    case 18: return  12;
    case 19: return  11;
    case 20: return  11;
    case 21: return  10;
    case 22: return  10;
    case 23: return   9;
    case 24: return   9;
    case 25: return   8;
    case 26: return   8;
    case 27: return   7;
    case 28: return   7;
    case 29: return   6;
    case 30: return   6;
    case 31: return   5;
    case 32: return   5;
    case 33: return   4;
    case 34: return   4;
    default:
      log("SYSERR: Missing level for thief,spy thac0.");
    }

  case CLASS_ASSASSIN:
  case CLASS_KNIGHT:
    switch (level) {
    case  0: return 100;
    case  1: return  20;
    case  2: return  19;
    case  3: return  18;
    case  4: return  17;
    case  5: return  16;
    case  6: return  15;
    case  7: return  14;
    case  8: return  14;
    case  9: return  13;
    case 10: return  12;
    case 11: return  11;
    case 12: return  10;
    case 13: return   9;
    case 14: return   8;
    case 15: return   7;
    case 16: return   6;
    case 17: return   5;
    case 18: return   4;
    case 19: return   3;
    case 20: return   2;
    case 21: return   1;
    case 22: return   1;
    case 23: return   1;
    case 24: return   1;
    case 25: return   1;
    case 26: return   1;
    case 27: return   1;
    case 28: return   1;
    case 29: return   1;
    case 30: return   1;
    case 31: return   1;
    case 32: return   1;
    case 33: return   1;
    case 34: return   1;
    default:
      log("SYSERR: Missing level for knight, assassin thac0.");
    }

  default:
    log("SYSERR: Unknown class in thac0 chart.");
  }

  /* Will not get there unless something is wrong. */
  return 100;
}


/*
 * Roll the 6 stats for a character... each stat is made of the sum of
 * the best 3 out of 4 rolls of a 6-sided die.  Each class then decides
 * which priority will be given for the best to worst stats.
 */
void roll_real_abils(struct char_data * ch)
{
/*  int i, j, k, temp;
  ubyte table[6];
  ubyte rolls[4];

  for (i = 0; i < 6; i++)
    table[i] = 0;

  for (i = 0; i < 6; i++) {

    for (j = 0; j < 4; j++)
      rolls[j] = number(1, 6);

    temp = rolls[0] + rolls[1] + rolls[2] + rolls[3] -
      MIN(rolls[0], MIN(rolls[1], MIN(rolls[2], rolls[3])));

    for (k = 0; k < 6; k++)
      if (table[k] < temp) {
	temp ^= table[k];
	table[k] ^= temp;
	temp ^= table[k];
      }
  }

  ch->real_abils.str_add = 0;

  switch (number(1,4)) {
  case 1:
    ch->real_abils.intel = table[0];
    ch->real_abils.wis = table[1];
    ch->real_abils.dex = table[2];
    ch->real_abils.str = table[3];
    ch->real_abils.con = table[4];
    ch->real_abils.cha = table[5];
    break;
  case 2:
    ch->real_abils.wis = table[0];
    ch->real_abils.intel = table[1];
    ch->real_abils.str = table[2];
    ch->real_abils.dex = table[3];
    ch->real_abils.con = table[4];
    ch->real_abils.cha = table[5];
    break;
  case 3:
    ch->real_abils.dex = table[0];
    ch->real_abils.str = table[1];
    ch->real_abils.con = table[2];
    ch->real_abils.intel = table[3];
    ch->real_abils.wis = table[4];
    ch->real_abils.cha = table[5];
    break;
  case 4:
    ch->real_abils.str = table[0];
    ch->real_abils.con = table[1];
    ch->real_abils.dex = table[2];
    ch->real_abils.wis = table[3];
    ch->real_abils.intel = table[4];
    ch->real_abils.cha = table[5];
    break;
  }

 if (ch->real_abils.str == 18)
  ch->real_abils.str_add = number(0, 100);
*/
 switch (GET_RACE(ch)) {                       
	case RACE_WERE:
	case RACE_HUMAN:                              
        /*if(ch->real_abils.str > 18)
			ch->real_abils.str = 18;
		if(ch->real_abils.dex > 18)
			ch->real_abils.dex = 18;
		if(ch->real_abils.con > 18)
			ch->real_abils.con = 18;
		if(ch->real_abils.wis > 18)
			ch->real_abils.wis = 18;
		if(ch->real_abils.intel > 18)
			ch->real_abils.intel =18;*/
			ch->real_abils.cha= 18;
		break;                                        
    case RACE_ELF: 
		/*if(ch->real_abils.str > 16)
			ch->real_abils.str = 16;
		if(ch->real_abils.con > 18)
			ch->real_abils.con = 18;
		if(ch->real_abils.wis > 19)
			ch->real_abils.wis = 19;
		if(ch->real_abils.intel > 18)
			ch->real_abils.intel = 18;
		if(ch->real_abils.dex > 20)
			ch->real_abils.dex = 20;*/
			ch->real_abils.cha = 24;
		break;                                        
    case RACE_GNOME:                              
		/*if(ch->real_abils.str > 17)
			ch->real_abils.str = 17;
		if(ch->real_abils.con > 16)
			ch->real_abils.con = 16;
		if(ch->real_abils.wis > 21)
			ch->real_abils.wis = 21;
		if(ch->real_abils.intel > 20)
			ch->real_abils.intel = 20;
		if(ch->real_abils.dex > 17)
			ch->real_abils.dex = 17*/;
			ch->real_abils.cha = 17;
		break;                                        
    case RACE_DWARF:
		/*if(ch->real_abils.str > 21)
			ch->real_abils.str = 21;
		if(ch->real_abils.con > 20)
			ch->real_abils.con = 20;
		if(ch->real_abils.wis > 16)
			ch->real_abils.wis = 16;
		if(ch->real_abils.intel > 15)
			ch->real_abils.intel = 15;
		if(ch->real_abils.dex > 17)
			ch->real_abils.dex = 17;*/
			ch->real_abils.cha = 17;
      break;
	case RACE_DRACO:
		/*if(ch->real_abils.str > 20)
			ch->real_abils.str = 20;
		if(ch->real_abils.con > 23)
			ch->real_abils.con = 23;
		if(ch->real_abils.wis > 21)
			ch->real_abils.wis = 21;
		if(ch->real_abils.intel > 19)
			ch->real_abils.intel = 19;
		if(ch->real_abils.dex > 16)
			ch->real_abils.dex = 16;*/
			ch->real_abils.cha = 15;
		break;
	case RACE_VAMPYRE:
		/*if(ch->real_abils.str > 20)
		    ch->real_abils.str = 20;
		if(ch->real_abils.con > 24)
			ch->real_abils.con = 24;
		if(ch->real_abils.wis > 18)
			ch->real_abils.wis = 18;
		if(ch->real_abils.intel > 18)
			ch->real_abils.intel = 18;
		if(ch->real_abils.dex > 19)
			ch->real_abils.dex = 19;*/
			ch->real_abils.cha = 20;
		break;
	case RACE_TROLL:
		/*if(ch->real_abils.str > 24)
			ch->real_abils.str = 24;
		if(ch->real_abils.con > 20)
			ch->real_abils.con = 20;
		if(ch->real_abils.wis > 15)
			ch->real_abils.wis = 15;
		if(ch->real_abils.intel > 13)
			ch->real_abils.intel = 13;
		if(ch->real_abils.dex > 15)
			ch->real_abils.dex = 15;*/
			ch->real_abils.cha = 12;
		break;
	case RACE_ARIEL:
		/*if(ch->real_abils.str > 18)
			ch->real_abils.str = 18;
		if(ch->real_abils.con > 17)
			ch->real_abils.con = 17;
		if(ch->real_abils.wis > 19)
			ch->real_abils.wis = 19;
		if(ch->real_abils.intel > 18)
			ch->real_abils.intel = 18;
		if(ch->real_abils.dex > 20)
			ch->real_abils.dex = 20;*/
			ch->real_abils.cha = 22;
		break;
	case RACE_FAIRY:
		/*if(ch->real_abils.str > 15)
			ch->real_abils.str = 15;
		if(ch->real_abils.con > 12)
			ch->real_abils.con = 12;
		if(ch->real_abils.wis > 20)
			ch->real_abils.wis = 20;
		if(ch->real_abils.intel > 21)
			ch->real_abils.intel = 21;
		if(ch->real_abils.dex > 21)
			ch->real_abils.dex = 21;*/
			ch->real_abils.cha = 24;
		break;
	case RACE_MINOTAUR:
		/*if(ch->real_abils.str > 20)
			ch->real_abils.str = 20;
		if(ch->real_abils.con > 20)
			ch->real_abils.con = 20;
		if(ch->real_abils.wis > 16)
			ch->real_abils.wis = 16;
		if(ch->real_abils.intel > 17)
			ch->real_abils.intel = 17;
		if(ch->real_abils.dex > 16)
			ch->real_abils.dex = 16;*/
			ch->real_abils.cha = 12;
		break;
	case RACE_GIANT:
		/*if(ch->real_abils.str > 24)
			ch->real_abils.str = 24;
		if(ch->real_abils.con > 23)
			ch->real_abils.con = 23;
		if(ch->real_abils.wis > 14)
			ch->real_abils.wis = 14;
		if(ch->real_abils.intel > 12)
			ch->real_abils.intel = 12;
		if(ch->real_abils.dex > 16)
			ch->real_abils.dex = 16;*/
			ch->real_abils.cha = 15;
		break;
	case RACE_GHOST:
		ch->real_abils.cha = 18;
		break;
	case RACE_GHOUL:
		ch->real_abils.cha = 12;
		break;
	case RACE_DROW:
		ch->real_abils.cha = 22;
		break;
	case RACE_KENDER:
		ch->real_abils.cha = 18;
		break;
	default:
		log("SYSERR: NO RACIAL MODS Race #%d", GET_RACE(ch));
      break;
   }         
   

	ch->real_abils.str = 12;
	ch->real_abils.con = 12;
	ch->real_abils.wis = 12;
	ch->real_abils.intel = 12;
	ch->real_abils.dex = 12;
	
  ch->aff_abils = ch->real_abils;
}


/* Some initializations for characters, including initial skills */
void do_start(struct char_data * ch)
{

  GET_LEVEL(ch) = 1;
  GET_EXP(ch) = 1;
  GET_RPP(ch) = 0;
  GET_QP(ch) = 0;

  set_title(ch, NULL);
  roll_real_abils(ch);
  ch->points.max_hit = 10;

  advance_level(ch);
  sprintf(buf, "%s advanced to level %d", GET_NAME(ch), GET_LEVEL(ch));
  mudlog(buf, BRF, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), TRUE);

  GET_HIT(ch) = GET_MAX_HIT(ch);
  GET_MANA(ch) = GET_MAX_MANA(ch);
  GET_MOVE(ch) = GET_MAX_MOVE(ch);

  GET_COND(ch, THIRST) = 24;
  GET_COND(ch, FULL) = 24;
  GET_COND(ch, DRUNK) = 0;

  ch->player.time.played = 0;
  ch->player.time.logon = time(0);

  if (siteok_everyone)
    SET_BIT(PLR_FLAGS(ch), PLR_SITEOK);
}



/*
 * This function controls the change to maxmove, maxmana, and maxhp for
 * each class every time they gain a level.
 */
void advance_level(struct char_data * ch)
{
  int add_hp, add_mana = 0, add_move = 0, i;

  add_hp = con_app[GET_CON(ch)].hitp;

  switch (GET_CLASS(ch)) {

  case CLASS_NONE:
	  add_hp += number(3, 15);
	  add_move = number(0, 4);
	  break;
  
  case CLASS_DEATH:
  case CLASS_LIFE:
  case CLASS_FIRE:
  case CLASS_WIND:
  case CLASS_WATER:
  case CLASS_EARTH:
  case CLASS_ILLUSION:
    add_hp += number(3, 8);
    add_mana = number(GET_GLEVEL(ch), (int) (1.5 * GET_GLEVEL(ch)));
    add_mana = MIN(add_mana, 10);
    add_move = number(0, 2);
    break;

  case CLASS_SPY:
  case CLASS_THIEF:
    add_hp += number(7, 13);
    add_move = number(1, 3);
    break;

  case CLASS_ASSASSIN:
    add_hp += number(8, 12);
    add_move = number(1, 3);
    break;

  case CLASS_KNIGHT:
    add_hp += number(10, 15);
    add_move = number(1, 3);
    break;
 
  case CLASS_GUARD:
  case CLASS_TRACKER:
    add_hp += number(7, 13);
    add_move = number(2, 4);
    break;

  case CLASS_BARD:
	  add_hp += number(5, 10);
	  add_move = number(10, 15);
	  break;
  }

  ch->points.max_hit += MAX(1, add_hp);
  ch->points.max_move += MAX(1, add_move);

  if (GET_LEVEL(ch) > 1)
    ch->points.max_mana += add_mana;

  if (GET_LEVEL(ch) >= LVL_IMMORT) {
    for (i = 0; i < 3; i++)
      GET_COND(ch, i) = (char) -1;
    SET_BIT(PRF_FLAGS(ch), PRF_HOLYLIGHT);
  }

  save_char(ch, NOWHERE);
}


/*
 * This simply calculates the backstab multiplier based on a character's
 * level.  This used to be an array, but was changed to be a function so
 * that it would be easier to add more levels to your MUD.  This doesn't
 * really create a big performance hit because it's not used very often.
 */
int backstab_mult(int level)
{
  if (level <= 0)
    return 1;	  /* level 0 */
  else if (level <= 7)
    return 2;	  /* level 1 - 7 */
  else if (level <= 13)
    return 3;	  /* level 8 - 13 */
  else if (level <= 20)
    return 4;	  /* level 14 - 20 */
  else if (level <= 29)
    return 5;	  /* level 21 - 28 */
  else if (level < LVL_IMMORT)
    return 6;	  /* all remaining mortal levels */
  else
    return 20;	  /* immortals */
}


/*
 * invalid_class is used by handler.c to determine if a piece of equipment is
 * usable by a particular class, based on the ITEM_ANTI_{class} bitvectors.
 */

int invalid_class(struct char_data *ch, struct obj_data *obj) {
  if ((IS_OBJ_STAT(obj, ITEM_ANTI_MAGIC_USER) && IS_MAGIC_USER(ch)) ||
      (IS_OBJ_STAT(obj, ITEM_ANTI_KNIGHT) && IS_KNIGHT(ch)) ||
      (IS_OBJ_STAT(obj, ITEM_ANTI_THIEF) && IS_THIEF(ch)) ||
	  (IS_OBJ_STAT(obj, ITEM_ANTI_ASSASSIN) && IS_ASSASSIN(ch)) ||
	  (IS_OBJ_STAT(obj, ITEM_ANTI_TRACKER) && IS_TRACKER(ch)) ||
	  (IS_OBJ_STAT(obj, ITEM_ANTI_MERC) && IS_MERC(ch)) ||
	  (IS_OBJ_STAT(obj, ITEM_ANTI_MERCHANT) && IS_MERCHANT(ch)) ||
	  (IS_OBJ_STAT(obj, ITEM_ANTI_BARD) && IS_BARD(ch)) ||
	  (IS_OBJ_STAT(obj, ITEM_ANTI_SPY) && IS_SPY(ch)) ||
	  (IS_OBJ_STAT(obj, ITEM_ANTI_GUARD) && IS_GUARD(ch)))
	return 1;
  else
	return 0;
}




/*
 * SPELLS AND SKILLS.  This area defines which spells are assigned to
 * which classes, and the minimum level the character must be to use
 * the spell or skill.
 */
void init_spell_levels(void)
{
	int i;

for (i = 0; i < NUM_CLASSES; i++){
	spell_level(SKILL_DAGGER, i, 1);
	spell_level(SKILL_DODGE, i, 1);
	spell_level(SKILL_SLING, i, 1);
	spell_level(SKILL_KICK, i, 1);
	spell_level(SKILL_PUNCH, i, 1);
	spell_level(SKILL_MOUNT, i, 1);
}

  spell_level(SKILL_MEDITATE, CLASS_DEATH, 1);
  spell_level(SKILL_MEDITATE, CLASS_LIFE, 1);
  spell_level(SKILL_MEDITATE, CLASS_FIRE, 1);
  spell_level(SKILL_MEDITATE, CLASS_WIND, 1);
  spell_level(SKILL_MEDITATE, CLASS_WATER, 1);
  spell_level(SKILL_MEDITATE, CLASS_EARTH, 1);
  spell_level(SKILL_MEDITATE, CLASS_ILLUSION, 1);

  /* THIEVES */
  spell_level(SKILL_PEEK, CLASS_THIEF, 1);
  spell_level(SKILL_SNEAK, CLASS_THIEF, 1);
  spell_level(SKILL_PICK_LOCK, CLASS_THIEF, 2);
  spell_level(SKILL_RETREAT, CLASS_THIEF, 3);
  spell_level(SKILL_STEAL, CLASS_THIEF, 4);
  spell_level(SKILL_HIDE, CLASS_THIEF, 5);
  spell_level(SKILL_THROW, CLASS_THIEF, 8);
  spell_level(SKILL_SECOND, CLASS_THIEF,10);
  
  /* KNIGHTS */
  spell_level(SKILL_BLOCK, CLASS_KNIGHT, 1);
  spell_level(SKILL_SWORD, CLASS_KNIGHT, 1);
  spell_level(SKILL_BLUDGEON, CLASS_KNIGHT, 1);
  spell_level(SKILL_RESCUE, CLASS_KNIGHT, 3);
  spell_level(SKILL_SECOND, CLASS_KNIGHT, 5);
  spell_level(SKILL_AXE, CLASS_KNIGHT, 9);
  spell_level(SKILL_BASH, CLASS_KNIGHT, 12);
  spell_level(SKILL_THIRD, CLASS_KNIGHT, 15);
  spell_level(SKILL_POLEARM, CLASS_KNIGHT, 18);
  spell_level(SKILL_FOURTH, CLASS_KNIGHT, 20);

  /*ASSASSINS*/
  spell_level(SKILL_BACKSTAB, CLASS_ASSASSIN, 1);
  spell_level(SKILL_SECOND, CLASS_ASSASSIN, 2);
  spell_level(SKILL_SWORD, CLASS_ASSASSIN, 3);
  spell_level(SKILL_SNEAK, CLASS_ASSASSIN, 4);
  spell_level(SKILL_HIDE, CLASS_ASSASSIN, 5);
  spell_level(SKILL_TRACK, CLASS_ASSASSIN, 5);
  spell_level(SKILL_BLUDGEON, CLASS_ASSASSIN, 8);
  spell_level(SKILL_THROW, CLASS_ASSASSIN, 10);
  spell_level(SKILL_CROSSBOW, CLASS_ASSASSIN, 15);
  spell_level(SKILL_THIRD, CLASS_ASSASSIN, 15);
}


/*
 * This is the exp given to implementors -- it must always be greater
 * than the exp required for immortality, plus at least 20,000 or so.
 */
#define EXP_MAX  10000000

/*Took out level_exp for leveless system*/

/* 
 * Default titles of male characters.
 */
const char *title_male(int chclass, int level)
{
  if (level <= 0 || level > LVL_IMPL)
    return "the Man";
  if (level == LVL_IMPL)
    return "the Implementor";

  switch (chclass) {

	case CLASS_NONE:
	switch(level){
      case LVL_IMMORT: return "the Immortal";
	  case LVL_HELPER: return "the Newbie Helper";
	  case LVL_QUESTOR: return "the Questor";
	  case LVL_BUILDER: return "the Builder";
	  case LVL_JUDGE: return "the Dispenser of Justice";
      case LVL_GOD: return "the Avatar";
      case LVL_GRGOD: return "the God";
      default: return "the Unranked";
	  }

    case CLASS_DEATH:
	case CLASS_LIFE:
	case CLASS_FIRE:
	case CLASS_WIND:
	case CLASS_WATER:
	case CLASS_EARTH:
	case CLASS_ILLUSION:
	case CLASS_ALCHEMIST:
    switch (level) {
      case LVL_IMMORT: return "the Immortal Warlock";
	  case LVL_HELPER: return "the Newbie Helper";
	  case LVL_QUESTOR: return "the Questor";
	  case LVL_BUILDER: return "the Builder";
	  case LVL_JUDGE: return "the Dispenser of Justice";
      case LVL_GOD: return "the Avatar of Magic";
      case LVL_GRGOD: return "the God of Magic";
      default: return "the Unranked";
    }
    break;

    case CLASS_THIEF:
    switch (level) {
      case LVL_IMMORT: return "the Immortal Thief";
	  case LVL_HELPER: return "the Newbie Helper";
	  case LVL_QUESTOR: return "the Questor";
	  case LVL_BUILDER: return "the Builder";
	  case LVL_JUDGE: return "the Dispenser of Justice";
      case LVL_GOD: return "the Demi God of Thieves";
      case LVL_GRGOD: return "the God of Thieves and Tradesmen";
      default: return "the Unranked";
    }
    break;

    case CLASS_KNIGHT:
    switch(level) {
      case LVL_IMMORT: return "the Immortal Warlord";
	  case LVL_HELPER: return "the Newbie Helper";
	  case LVL_QUESTOR: return "the Questor";
	  case LVL_BUILDER: return "the Builder";
	  case LVL_JUDGE: return "the Dispenser of Justice";
      case LVL_GOD: return "the Extirpator";
      case LVL_GRGOD: return "the God of War";
      default: return "the Unranked";
    }
    break;
    
	case CLASS_ASSASSIN:
    switch (level) {
      case LVL_IMMORT: return "the Immortal Assassin";
	  case LVL_HELPER: return "the Newbie Helper";
	  case LVL_QUESTOR: return "the Questor";
	  case LVL_BUILDER: return "the Builder";
	  case LVL_JUDGE: return "the Dispenser of Justice";
      case LVL_GOD: return "the Demi God of the Dark Arts";
      case LVL_GRGOD: return "the God of Assassins and Mercenaries";
      default: return "the Unranked";
    }
    break;

	case CLASS_TRACKER:
    switch (level) {
      case LVL_IMMORT: return "the Immortal Tracker";
	  case LVL_HELPER: return "the Newbie Helper";
	  case LVL_QUESTOR: return "the Questor";
	  case LVL_BUILDER: return "the Builder";
	  case LVL_JUDGE: return "the Dispenser of Justice";
      case LVL_GOD: return "the Woodsman";
      case LVL_GRGOD: return "the God of the Woods";
      default: return "the Unranked";
    }
    break;

	case CLASS_SPY:
	switch (level) {
      case LVL_IMMORT: return "the Immortal Spy";
	  case LVL_HELPER: return "the Newbie Helper";
	  case LVL_QUESTOR: return "the Questor";
	  case LVL_BUILDER: return "the Builder";
	  case LVL_JUDGE: return "the Dispenser of Justice";
      case LVL_GOD: return "the Demi God of the Sly";
      case LVL_GRGOD: return "the God of the Dreaded";
      default: return "the Unranked";
    }
	break;

	case CLASS_GUARD:
	switch (level) {
      case LVL_IMMORT: return "the Immortal Guard";
	  case LVL_HELPER: return "the Newbie Helper";
	  case LVL_QUESTOR: return "the Questor";
	  case LVL_BUILDER: return "the Builder";
	  case LVL_JUDGE: return "the Dispenser of Justice";
      case LVL_GOD: return "the Guard of All";
      case LVL_GRGOD: return "the God of the Protectors";
      default: return "the Unranked";
    }
	break;
  }

  /* Default title for classes which do not have titles defined */
  return "the Classless";
}


/* 
 * Default titles of female characters.
 */
const char *title_female(int chclass, int level)
{
  if (level <= 0 || level > LVL_IMPL)
    return "the Woman";
  if (level == LVL_IMPL)
    return "the Implementress";

  switch (chclass) {

	case CLASS_NONE:
	switch(level){
      case LVL_IMMORT: return "the Immortal";
	  case LVL_HELPER: return "the Newbie Helper";
	  case LVL_QUESTOR: return "the Questor";
	  case LVL_BUILDER: return "the Builder";
	  case LVL_JUDGE: return "the Dispenser of Justice";
      case LVL_GOD: return "the Avatar";
      case LVL_GRGOD: return "the God";
      default: return "the Unranked";
	  }

    case CLASS_DEATH:
	case CLASS_LIFE:
	case CLASS_FIRE:
	case CLASS_WIND:
	case CLASS_WATER:
	case CLASS_EARTH:
	case CLASS_ILLUSION:
	case CLASS_ALCHEMIST:
    switch (level) {
      case LVL_IMMORT: return "the Immortal Enchantress";
	  case LVL_HELPER: return "the Newbie Helper";
	  case LVL_QUESTOR: return "the Questor";
	  case LVL_BUILDER: return "the Builder";
	  case LVL_JUDGE: return "the Dispenser of Justice";
      case LVL_GOD: return "the Empress of Magic";
      case LVL_GRGOD: return "the Goddess of Magic";
      default: return "the Unranked";
    }
    break;

    case CLASS_THIEF:
    switch (level) {
      case LVL_IMMORT: return "the Immortal Thief";
	  case LVL_HELPER: return "the Newbie Helper";
	  case LVL_QUESTOR: return "the Questor";
	  case LVL_BUILDER: return "the Builder";
	  case LVL_JUDGE: return "the Dispenser of Justice";
      case LVL_GOD: return "the Demi Goddess of the Quick Handed";
      case LVL_GRGOD: return "the Goddess of Thieves";
      default: return "the Unranked";
    }
    break;

    case CLASS_KNIGHT:
    switch(level) {
      case LVL_IMMORT: return "the Immortal Lady of War";
	  case LVL_HELPER: return "the Newbie Helper";
	  case LVL_QUESTOR: return "the Questor";
	  case LVL_BUILDER: return "the Builder";
	  case LVL_JUDGE: return "the Dispenser of Justice";
      case LVL_GOD: return "the Queen of Destruction";
      case LVL_GRGOD: return "the Goddess of War";
      default: return "the Unranked";
    }
    break;

    case CLASS_ASSASSIN:
    switch (level) {
      case LVL_IMMORT: return "the Immortal Assassin";
	  case LVL_HELPER: return "the Newbie Helper";
	  case LVL_QUESTOR: return "the Questor";
	  case LVL_BUILDER: return "the Builder";
	  case LVL_JUDGE: return "the Dispenser of Justice";
      case LVL_GOD: return "the Demi Goddess of the Dark Arts";
      case LVL_GRGOD: return "the Goddess of Assassins";
      default: return "the Unranked";
    }
    break;

	case CLASS_TRACKER:
    switch (level) {
      case LVL_IMMORT: return "the Immortal Tracker";
	  case LVL_HELPER: return "the Newbie Helper";
	  case LVL_QUESTOR: return "the Questor";
	  case LVL_BUILDER: return "the Builder";
	  case LVL_JUDGE: return "the Dispenser of Justice";
      case LVL_GOD: return "the Wood Nymph";
      case LVL_GRGOD: return "the Goddess of the Woods";
      default: return "the Unranked";
    }
    break;

	case CLASS_SPY:
	switch (level) {
      case LVL_IMMORT: return "the Immortal Spy";
	  case LVL_HELPER: return "the Newbie Helper";
	  case LVL_QUESTOR: return "the Questor";
	  case LVL_BUILDER: return "the Builder";
	  case LVL_JUDGE: return "the Dispenser of Justice";
      case LVL_GOD: return "the Demi Goddess of the Sly";
      case LVL_GRGOD: return "the Goddess of the Dreaded";
      default: return "the Unranked";
    }
	break;

	case CLASS_GUARD:
	switch (level) {
      case LVL_IMMORT: return "the Immortal Guard";
	  case LVL_HELPER: return "the Newbie Helper";
	  case LVL_QUESTOR: return "the Questor";
	  case LVL_BUILDER: return "the Builder";
	  case LVL_JUDGE: return "the Dispenser of Justice";
      case LVL_GOD: return "the Guard of All";
      case LVL_GRGOD: return "the Goddess of the Protectors";
      default: return "the Unranked";
    }
	break;
  }

  /* Default title for classes which do not have titles defined */
  return "the Classless";
}

int train_params[6][NUM_RACES] = {
  /* HUM   ELF   DWA   DRC   WER   VAM   TRL   ARL   FAI   MIN   GIA   GNO   GHO   GHU   DRO   KEN*/    
  {  18,   16,   21,   20,   18,   20,   24,   18,   12,   20,   24,   17,   15,   19,   20,   13},   /* Strength */
  {  18,   17,   20,   23,   18,   24,   20,   16,   12,   20,   23,   16,   21,   25,   18,   15},   /* Constitution */
  {  18,   19,   16,   21,   18,   18,   15,   19,   20,   16,   14,   21,   23,   16,   17,   17},   /* Wisdom */
  {  18,   18,   15,   19,   18,   18,   13,   18,   21,   17,   12,   20,   22,   16,   16,   16},   /* Intelligence */
  {  18,   20,   17,   16,   18,   19,   15,   20,   20,   16,   16,   17,   12,   15,   20,   25},   /* Dexterity */
  {  18,   23,   17,   15,   18,   20,   12,   24,   25,   12,   15,   17,   18,   12,   22,   18}    /* Charisma */
};

ACMD(do_gaccept){
	
	struct char_data *vict;

	one_argument(argument, arg);

	if(GET_GLEVEL(ch) != 30){
		send_to_char("Only the Guild Leader can do that.",ch);
		return;
	}
	
	if (*arg) {
		if (!(vict = get_char_vis(ch, arg, FIND_CHAR_WORLD))) {
			send_to_char("That player is not here.\r\n", ch);
			return;
		}
	} else {
	 send_to_char("Guild who?\r\n", ch);
	 return;
	 }

	if(GET_CLASS(vict) > 0){
		send_to_char("You can not guild someone of another guild.\r\n",ch);
		return;
	}
	
	else if(GET_EXP(ch) <= 17500 && GET_RPP(ch) <= 1000){
		send_to_char("You can not guild one who has no identity.\r\nIf you believe this does not apply, TALK TO AN IMM!\r\n",ch);
		send_to_char("You can not be guilded with out an identity.\r\nIf you believe this does not apply, TALK TO AN IMM!\r\n",vict);
		return;
	}
	else {
		sprintf(buf,"You are accepted into %s Guild.\r\n", CLASS_NAME(ch));
		send_to_char(buf, vict);
		sprintf(buf, "You accept %s into your Guild.\r\n", GET_NAME(vict));
		send_to_char(buf, ch);

		GET_CLASS(vict) = GET_CLASS(ch);
		GET_GLEVEL(vict) = 1;
	}
}

ACMD(do_gadvance){
	
	struct char_data *vict;
	one_argument(argument, arg);

	
	if(GET_GLEVEL(ch) != 30){
		send_to_char("Only the Guild Leader can do that.",ch);
		return;
	}

	if (*arg) {
		if (!(vict = get_char_vis(ch, arg, FIND_CHAR_WORLD))) {
			send_to_char("That player is not here.\r\n", ch);
			return;
		}
	} else {
	 send_to_char("Advance who?\r\n", ch);
	 return;
	 }

	if(GET_CLASS(vict) == 0){
		send_to_char("You can not advance someone of no guild.\r\n",ch);
		return;
	}

	if(GET_CLASS(vict) != GET_CLASS(ch)){
		send_to_char("You can not advance those of another guild.\r\n", ch);
		return;
	}

	if(GET_GLEVEL(vict) + 1 == GET_GLEVEL(ch)){
		send_to_char("Only Imms can make Guild Leaders.\r\n", ch);
		return;
	}

  sprintf(buf,"You are advanced by %s.\r\n", GET_NAME(ch));
  send_to_char(buf, vict);
  sprintf(buf, "You advance %s to level %d.\r\n", GET_NAME(vict), GET_GLEVEL(vict) + 1);
  send_to_char(buf, ch);

  GET_GLEVEL(vict) += 1;
}

ACMD(do_gdemote){
	
	struct char_data *vict;
	one_argument(argument, arg);

	
	if(GET_GLEVEL(ch) != 30){
		send_to_char("Only the Guild Leader can do that.",ch);
		return;
	}

	if (*arg) {
		if (!(vict = get_char_vis(ch, arg, FIND_CHAR_WORLD))) {
			send_to_char("That player is not here.\r\n", ch);
			return;
		}
	} else {
	 send_to_char("Demote who?\r\n", ch);
	 return;
	 }

	if(GET_CLASS(vict) == 0){
		send_to_char("You can not advance someone of no guild.\r\n",ch);
		return;
	}

	if(GET_CLASS(vict) != GET_CLASS(ch)){
		send_to_char("You can not advance those of another guild.\r\n", ch);
		return;
	}

	if(GET_GLEVEL(vict) - 1 == 0){
		send_to_char("You can not demote them any further. Expel them!\r\n", ch);
		return;
	}

  sprintf(buf,"You are demoted by %s.\r\n", GET_NAME(ch));
  send_to_char(buf, vict);
  sprintf(buf, "You demote %s to level %d.\r\n", GET_NAME(vict), GET_GLEVEL(vict) - 1);
  send_to_char(buf, ch);

  GET_GLEVEL(vict) -= 1;
}

ACMD(do_gexpel){
	struct char_data *vict;
	int newlevel = 0;
	one_argument(argument, arg);

	
	if(GET_GLEVEL(ch) != 30){
		send_to_char("Only the Guild Leader can do that.",ch);
		return;
	}

	if (*arg) {
		if (!(vict = get_char_vis(ch, arg, FIND_CHAR_WORLD))) {
			send_to_char("That player is not here.\r\n", ch);
			return;
		}
	} else {
	 send_to_char("Guild expel who?\r\n", ch);
	 return;
	 }

if(GET_CLASS(vict) > 0){
		send_to_char("You can not expel someone of another guild.\r\n",ch);
		return;
	}

 

  sprintf(buf,"You are expeled by %s.\r\n", GET_NAME(ch));
  send_to_char(buf, vict);
  sprintf(buf, "You expel %s from the guild.\r\n", GET_NAME(vict), newlevel);
  send_to_char(buf, ch);
  sprintf(buf, "%s expeled %s from %s", GET_NAME(ch), GET_NAME(vict), CLASS_NAME(ch));
  mudlog(buf, BRF, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), TRUE);

  GET_CLASS(vict) = 0;
  GET_GLEVEL(vict) = newlevel;
}
