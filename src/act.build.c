/*
 * This has not yet been integrated to use the Generic OLC routines.
 * If you'd like to modify it to do so, please send patches to
 * George Greer <greerga@circlemud.org>. The advantage being, of course,
 * that you can use OasisOLC and OBuild at the same time with ease.
 */
#if 0
/*************************************************************************
*   File: act.build.c version .08		        Part of CircleMUD *
*  Usage: online editing/creating                                         *
*  By Samedi                                                              *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
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
#include "house.h"
#include "screen.h"

#include "genolc.h"

struct obuild_olc_data {
  int zone_edit;	/* PC Zone being edited		*/
  int room_edit;	/* PC Zone being edited		*/
  int zone_flags;	/* PC Zone flags		*/
  int default_zmob;	/* PC Zone mob default		*/
  struct obj_data *obj;	/*PC object edit buffer		*/
  struct char_data *mob;/*PC mob edit buffer		*/
  sh_int default_mob;	/* PC OLC default mob proto	*/
  sh_int default_obj;	/* PC OLC default obj proto	*/
  sh_int default_room;	/* PC OLC default room proto	*/
}

/*   external vars  */
extern FILE *player_fl;
extern struct room_data *world;
extern struct char_data *character_list;
extern struct obj_data *object_list;
extern struct descriptor_data *descriptor_list;
extern struct index_data *mob_index;
extern struct index_data *obj_index;
extern struct zone_data *zone_table;
extern zone_rnum top_of_zone_table;
extern room_rnum top_of_world;
extern mob_rnum top_of_mobt;
extern obj_rnum top_of_objt;

/* for objects */
extern char *item_types[];
extern char *wear_bits[];
extern char *extra_bits[];
extern char *drinks[];
extern struct obj_data *obj_proto;
extern int max_new_objects;

/* for rooms */
extern char *dirs[];
extern char *room_bits[];
extern char *exit_bits[];
extern char *sector_types[];
extern int max_new_rooms;
extern int rev_dir[];

/* for zones */
extern int comment_zone_file;

/* for chars */
extern struct char_data *mob_proto;
extern int max_new_mobs;

/* internal for object editing */
int new_objects;
int get_line2(FILE * fl, char *buf);
int get_zone_perms(struct char_data * ch, int z_num);
int check_edit_obj(struct char_data * ch, int obj_vnum);
int obj_allow = TRUE;
int objs_to_file(int zone);
int copy_object(struct obj_data * dest, struct obj_data * src);
void fix_obj_strings(struct obj_data * proto, struct obj_data * buffer);

/*room editing*/
int new_rooms;
int room_allow = TRUE;
int add_to_index(int zone, char * type);
int save_rooms(int zone);
int check_edit_zrooms(struct char_data * ch, int zone_vnum);
int copy_room(sh_int dest, sh_int src);
int create_dir(int room, int dir);
int free_dir(int room, int dir);

/*mob editing*/
int mob_allow = TRUE;
int new_mobs;
int check_edit_mob(struct char_data * ch, int mob_vnum);
void save_mob(struct char_data * ch);
int mobs_to_file(int zone);
int copy_mobile(struct char_data * dest, struct char_data * src);
void fix_mob_strings(struct char_data * proto, struct char_data * buffer);

/*zone editing*/
int new_zones;
int zone_allow = TRUE;
struct zone_data *add_zone(struct zone_data * zn_table, int zone);
int get_top(int zone);
int get_zon_num(int vnum);
int get_zone_rnum(int zone);
int read_zone_perms(int rnum);
void show_z_resets(struct char_data * ch, int min, int max);
void delete_reset_com(int zone, int com_num);
void add_reset_com(int zone, int com_num, struct reset_com * cmd);
void save_zone(struct char_data * ch, int zone);
void sprintbits(long vektor,char *outstring);
int check_edit_zone(struct char_data * ch, int zone_vnum);
void kill_ems(char *str);

#define OLC(ch)			((struct obuild_olc_data *)(ch)->desc->olc)
#define OBJ_NEW(ch)		(OLC(ch)->obj)
#define MOB_NEW(ch)		(OLC(ch)->mob)
#define ZONE_BUF(ch)		(OLC(ch)->zone)
#define ROOM_BUF(ch)		(OLC(ch)->room)
#define GET_ROOM_SECTOR(room)	(world[(room)].sector_type)
#define ZON_FLAGS(ch)		(OLC(ch)->zone_flags?)
#define ZON_FLAGGED(ch, flag)	(IS_SET(ZON_FLAGS(ch), (flag)))
#define CHECK_NULL(string)	((string) ? (string) : "")
#define DEF_MOB(ch)		((ch)->player.default_mob)
#define DEF_OBJ(ch)		((ch)->player.default_obj)
#define DEF_ROOM(ch)		((ch)->player.default_room)

void do_stat_object(struct char_data * ch, struct obj_data * j);
void do_stat_character(struct char_data * ch, struct char_data * k);
void do_stat_room(struct char_data * ch); 

ACMD(do_ostat)
{
  struct obj_data *obj;
  int number, r_num;

  one_argument(argument, buf);

  if(*buf) {
    if (!isdigit(*buf)) {
      send_to_char("Usage: ostat [vnum]\r\n", ch);
      return;
    }

    if ((number = atoi(buf)) < 0) {
      send_to_char("A NEGATIVE number??\r\n", ch);
      return;
    }

    if ((r_num = real_object(number)) < 0) {
      send_to_char("There is no object with that number.\r\n", ch);
      return;
    }
    obj = read_object(r_num, REAL);
    do_stat_object(ch, obj);
    extract_obj(obj);
  } else if (!OBJ_NEW(ch)) {
    send_to_char("There is no default object to stat.\r\n", ch);
    return;
  } else {
    do_stat_object(ch, OBJ_NEW(ch));
  }
}


ACMD(do_mstat)
{
  struct char_data *mob;
  int number, r_num;
  int old_hit, old_mana, old_move; /* for true hp on buffer mobs */

  one_argument(argument, buf);

  if(*buf) {
    if (!isdigit(*buf)) {
      send_to_char("Usage: mstat [vnum]\r\n", ch);
      return;
    }

    if ((number = atoi(buf)) < 0) {
      send_to_char("A NEGATIVE number??\r\n", ch);
      return;
    }

    if ((r_num = real_mobile(number)) < 0) {
      send_to_char("There is no mob with that number.\r\n", ch);
      return;
    }
    mob = read_mobile(r_num, REAL);
    char_to_room(mob, 0);
    do_stat_character(ch, mob);
    extract_char(mob);
  } else if (!MOB_NEW(ch)) {
    send_to_char("There is no default mob to stat.\r\n", ch);
    return;
  } else {
    if(!MOB_NEW(ch)) {
      send_to_char("No mob in the edit buffer.\r\n", ch);
      return;
    }
    old_hit = MOB_NEW(ch)->points.hit;
    old_mana = MOB_NEW(ch)->points.mana;
    old_move = MOB_NEW(ch)->points.move;
    MOB_NEW(ch)->points.max_hit = dice(MOB_NEW(ch)->points.hit,
      MOB_NEW(ch)->points.mana) + MOB_NEW(ch)->points.move;
    MOB_NEW(ch)->points.hit = MOB_NEW(ch)->points.max_hit;
    MOB_NEW(ch)->points.mana = MOB_NEW(ch)->points.max_mana;
    MOB_NEW(ch)->points.move = MOB_NEW(ch)->points.max_move;
    do_stat_character(ch, MOB_NEW(ch));
    MOB_NEW(ch)->points.max_hit = 0;
    MOB_NEW(ch)->points.hit = old_hit;
    MOB_NEW(ch)->points.mana = old_mana;
    MOB_NEW(ch)->points.move = old_move;
  }
}



ACMD(do_rstat)
{
  int number, r_num, room_temp;

  one_argument(argument, buf);

  if(*buf) {
    if (!isdigit(*buf)) {
      send_to_char("Usage: rstat [vnum]\r\n", ch);
      return;
    }

    if ((number = atoi(buf)) < 0) {
      send_to_char("A NEGATIVE number??\r\n", ch);
      return;
    }

    if ((r_num = real_room(number)) < 0) {
      send_to_char("There is no room with that number.\r\n", ch);
      return;
    }
    room_temp = ch->in_room;
    char_from_room(ch);
    char_to_room(ch, r_num);
    do_stat_room(ch);
    char_from_room(ch);
    char_to_room(ch, room_temp);
  } else {
    do_stat_room(ch);
  }
}



ACMD(do_zstat)
{
  int i, zn = -1, pos = 0, none = TRUE;
  struct builder_list *bldr;

  char *rmode[] =
    {  "Never resets",
       "Resets only when deserted",
       "Always resets"  };

  one_argument(argument, buf);

  if(*buf) {
    if(isdigit(*buf))
      zn = atoi(buf);
    else {
      send_to_char("Usage: zstat [zone number]\r\n", ch);
      return;
    }
  } else zn = get_zon_num(world[ch->in_room].number);
  
  if(zn < 0) {
    sprintf(buf, "SYSERR: Zone #%d doesn't seem to exist.",
      world[ch->in_room].number / 100);
    log(buf);
    send_to_char("Warning!  The room you're in doesn't belong to any zone in memory.\r\n", ch);
    return;
  }

  for(i = 0; i <= top_of_zone_table && zone_table[i].number != zn; i++);
  if(i > top_of_zone_table) {
    send_to_char("That zone doesn't exist.\r\n", ch);
    return;
  }

  if((zone_table[i].reset_mode < 0) || (zone_table[i].reset_mode > 2))
    zone_table[i].reset_mode = 2;
 
  sprintf(buf, "%sZone #      : %s%3d       %sName      : %s%s\r\n", 
    CCMAG(ch, C_SPR),
    CCWHT(ch, C_SPR), zone_table[i].number, CCMAG(ch, C_SPR),
    CCWHT(ch, C_SPR), zone_table[i].name);
  sprintf(buf, "%s%sAge/Lifespan: %s%3d%s/%s%3d   %sTop vnum  : %s%5d\r\n",
    buf, CCMAG(ch, C_SPR), CCWHT(ch, C_SPR), zone_table[i].age,
    CCMAG(ch, C_SPR), CCWHT(ch, C_SPR), zone_table[i].lifespan,
    CCMAG(ch, C_SPR), CCWHT(ch, C_SPR), zone_table[i].top);
  sprintf(buf, "%s%sReset mode  : %s%s\r\n", buf, CCMAG(ch, C_SPR),
    CCWHT(ch, C_SPR), rmode[zone_table[i].reset_mode]);
  sprintf(buf, "%s%sBuilders    : %s", buf, CCMAG(ch, C_SPR), CCWHT(ch, C_SPR));

  if(zone_table[i].builders == NULL)
    read_zone_perms(i);

  for(bldr = zone_table[i].builders; bldr; bldr = bldr->next) {
    if(*(bldr->name) != '*') {
	none = FALSE;
	switch (pos) {
	  case 0:
	    sprintf(buf, "%s%s", buf, bldr->name);
	    pos = 2;
	    break;
	  case 1:
	    sprintf(buf, "%s\r\n              %s", buf, bldr->name);
	    pos = 2;
	    break;
	  case 2:
	    sprintf(buf, "%s, %s", buf, bldr->name);
	    pos = 3;
	    break;
	  case 3:
	    sprintf(buf, "%s, %s", buf, bldr->name);
	    pos = 1;
	    break;
	}
    }
  }

  if(none) sprintf(buf, "%sNONE%s\r\n", buf, CCNRM(ch, C_SPR));
  else sprintf(buf, "%s%s\r\n", buf, CCNRM(ch, C_SPR));
  send_to_char(buf, ch);
}


ACMD(do_zflags)
{
  int cmmd = 0, length = 0, help = FALSE;
  char field[MAX_INPUT_LENGTH];
  char val_arg[MAX_INPUT_LENGTH];
  int min = -1, max = -1;

  char *fields[] =
    { "show"            ,"none"         ,"doors"        ,
      "all"             ,"mobs"         ,"oloads"       ,
      "gives"           ,"equips"       ,"puts"         ,
      "removes"         ,"default"      ,
      "\n" };

  if(!*argument && ZON_FLAGS(ch) != 0) {
    show_z_resets(ch, -1, -1);
    return;
  }
  half_chop(argument, field, val_arg);
  if(!*field) *field = '*';

  if(isdigit(*field))
    min = atoi(field);
  if((*val_arg) && isdigit(*val_arg))
    max = atoi(val_arg);

  if((min > -1) && ((max == -1) || (max >= min))) {
    show_z_resets(ch, min, max);
    return;
  }

  do {
    for (length = strlen(field), cmmd = 0; *fields[cmmd] != '\n'; cmmd++) 
      if (!strn_cmp(fields[cmmd], field, length))  
	break;

    switch(cmmd) {
      case 0:
	send_to_char("You currently have the following flags ON.\r\n", ch);
	if(!ZON_FLAGS(ch)) {
	  send_to_char("  All flags are OFF\r\n", ch);
	  return;
	}
	if(ZON_FLAGGED(ch, ZFL_DOORS))
	  send_to_char("  Doors are ON\r\n", ch);
	if(ZON_FLAGGED(ch, ZFL_MOBS))
	  send_to_char("  Mobs are ON\r\n", ch);
	if(ZON_FLAGGED(ch, ZFL_OLOADS))
	  send_to_char("  Objects loaded to the room are ON\r\n", ch);
	if(ZON_FLAGGED(ch, ZFL_OGIVES))
	  send_to_char("  Objects given to mobs are ON\r\n", ch);
	if(ZON_FLAGGED(ch, ZFL_OEQUIPS))
	  send_to_char("  Objects equipped by mobs are ON\r\n", ch);
	if(ZON_FLAGGED(ch, ZFL_OPUTS))
	  send_to_char("  Objects put in other objects are ON\r\n", ch);
	if(ZON_FLAGGED(ch, ZFL_REMOVES))
	  send_to_char("  Objects removed from the room are ON\r\n", ch);
	if(ZON_FLAGGED(ch, ZFL_DEFAULT))
	  send_to_char("  The default mob is ON\r\n", ch);
	return;
      case 1:
	send_to_char("All flags turned OFF\r\n", ch);
	ZON_FLAGS(ch) = 0;
	break;
      case 2:
	TOGGLE_BIT(ZON_FLAGS(ch), ZFL_DOORS);
	if(IS_SET(ZON_FLAGS(ch), ZFL_DOORS))
	  send_to_char("Doors turned ON\r\n", ch);
	else
	  send_to_char("Doors turned OFF\r\n", ch);
	break;
      case 3:/*all on*/
	SET_BIT(ZON_FLAGS(ch), ZFL_DOORS | ZFL_MOBS | ZFL_OLOADS |
	  ZFL_OGIVES | ZFL_OEQUIPS | ZFL_OPUTS | ZFL_REMOVES | ZFL_DEFAULT);
	send_to_char("All flags turned ON\r\n", ch);
	break;
      case 4:
	TOGGLE_BIT(ZON_FLAGS(ch), ZFL_MOBS);
	if(IS_SET(ZON_FLAGS(ch), ZFL_MOBS))
	  send_to_char("Mobs turned ON\r\n", ch);
	else
	  send_to_char("Mobs turned OFF\r\n", ch);
	break;
      case 5:
	TOGGLE_BIT(ZON_FLAGS(ch), ZFL_OLOADS);
	if(IS_SET(ZON_FLAGS(ch), ZFL_OLOADS))
	  send_to_char("Object loads turned ON\r\n", ch);
	else
	  send_to_char("Object loads turned OFF\r\n", ch);
	break;
      case 6:
	TOGGLE_BIT(ZON_FLAGS(ch), ZFL_OGIVES);
	if(IS_SET(ZON_FLAGS(ch), ZFL_OGIVES))
	  send_to_char("Object gives turned ON\r\n", ch);
	else
	  send_to_char("Object gives turned OFF\r\n", ch);
	break;
      case 7:
	TOGGLE_BIT(ZON_FLAGS(ch), ZFL_OEQUIPS);
	if(IS_SET(ZON_FLAGS(ch), ZFL_OEQUIPS))
	  send_to_char("Object equips turned ON\r\n", ch);
	else
	  send_to_char("Object equips turned OFF\r\n", ch);
	break;
      case 8:
	TOGGLE_BIT(ZON_FLAGS(ch), ZFL_OPUTS);
	if(IS_SET(ZON_FLAGS(ch), ZFL_OPUTS))
	  send_to_char("Object puts turned ON\r\n", ch);
	else
	  send_to_char("Object puts turned OFF\r\n", ch);
	break;
      case 9:
	TOGGLE_BIT(ZON_FLAGS(ch), ZFL_REMOVES);
	if(IS_SET(ZON_FLAGS(ch), ZFL_REMOVES))
	  send_to_char("Object removes turned ON\r\n", ch);
	else
	  send_to_char("Object removes turned OFF\r\n", ch);
	break;
      case 10:
	TOGGLE_BIT(ZON_FLAGS(ch), ZFL_DEFAULT);
	if(IS_SET(ZON_FLAGS(ch), ZFL_DEFAULT))
	  send_to_char("Default mob turned ON\r\n", ch);
	else
	  send_to_char("Default mob turned OFF\r\n", ch);
	break;
      default:
	if(!help) {
	  send_to_char("Usage   : zflags [flag]\r\n\r\n", ch);
	  send_to_char("Valid flags are:\r\n\r\n", ch);
	  send_to_char("show         show status of flags\r\n", ch);
	  send_to_char("all          turn all flags ON\r\n", ch);
	  send_to_char("none         turn all flags OFF\r\n", ch);
	  send_to_char("doors        show door resets\r\n", ch);
	  send_to_char("mobs         show mob resets\r\n", ch);
	  send_to_char("oloads       show objects that load into the room\r\n", ch);
	  send_to_char("gives        show objects given to mobs\r\n", ch);
	  send_to_char("equips       show objects equipped on mobs\r\n", ch);
	  send_to_char("puts         show objects put in other objects\r\n", ch);
	  send_to_char("removes      show objects removed from the room\r\n", ch);
	  send_to_char("default      show the default mob\r\n", ch);
	  send_to_char("help         show this list\r\n\r\n", ch);
	  send_to_char("Zflags will accept multiple flags on the same line.  Zflags with no arguments\r\n", ch);
	  send_to_char("will show the resets in the current room.\r\n", ch);
	  help = TRUE;
	}
    }
    half_chop(val_arg, field, val_arg);
  } while(*field);
}


/* load a copy of the object being edited to the editor's 
 * inventory
*/
ACMD(do_oload)
{
  int number, r_num, i;
  struct obj_data *obj; 
  struct extra_descr_data *desc, *desc2;

  one_argument(argument, buf);
  
  if(!*buf && !OBJ_NEW(ch)) {
    send_to_char("There is no object in the edit buffer.  Use oedit to set one.\r\n", ch);
    return;
  } else if (!*buf) {
    sprintf(buf, "Poof!  You create %s.\r\n", OBJ_NEW(ch)->short_description);
    send_to_char(buf, ch);
    obj = read_object(OBJ_NEW(ch)->item_number, REAL);
    if(OBJ_NEW(ch)->name)
      obj->name = str_dup(OBJ_NEW(ch)->name);
    if(OBJ_NEW(ch)->short_description)
     obj->short_description = str_dup(OBJ_NEW(ch)->short_description);
    if(OBJ_NEW(ch)->description)
      obj->description = str_dup(OBJ_NEW(ch)->description);
    if(OBJ_NEW(ch)->action_description)
      obj->action_description = str_dup(OBJ_NEW(ch)->action_description);
    obj->obj_flags.type_flag = OBJ_NEW(ch)->obj_flags.type_flag;
    obj->obj_flags.extra_flags = OBJ_NEW(ch)->obj_flags.extra_flags;
    obj->obj_flags.wear_flags = OBJ_NEW(ch)->obj_flags.wear_flags;
    obj->obj_flags.value[0] = OBJ_NEW(ch)->obj_flags.value[0];
    obj->obj_flags.value[1] = OBJ_NEW(ch)->obj_flags.value[1];
    obj->obj_flags.value[2] = OBJ_NEW(ch)->obj_flags.value[2];
    obj->obj_flags.value[3] = OBJ_NEW(ch)->obj_flags.value[3];
    obj->obj_flags.weight = OBJ_NEW(ch)->obj_flags.weight;
    obj->obj_flags.cost = OBJ_NEW(ch)->obj_flags.cost;
    obj->obj_flags.cost_per_day = OBJ_NEW(ch)->obj_flags.cost_per_day;
    obj->ex_description = NULL;
    if(OBJ_NEW(ch)->ex_description) {
      for(desc = OBJ_NEW(ch)->ex_description; desc; desc = desc->next) {
	CREATE(desc2, struct extra_descr_data, 1);
	desc2->keyword = str_dup(desc->keyword);
	desc2->description = str_dup(desc->description);
	desc2->next = obj->ex_description;
	obj->ex_description = desc2;
      }
    }
    for(i = 0; i < MAX_OBJ_AFFECT; i++) {
      obj->affected[i].location = OBJ_NEW(ch)->affected[i].location;
      obj->affected[i].modifier = OBJ_NEW(ch)->affected[i].modifier;
    }
    obj_to_char(obj, ch);
    sprintf(buf2, "load: obj #%i oloaded by %s", GET_OBJ_VNUM(obj),
      GET_NAME(ch));
    log(buf2);
    return;
  } 
 
  if(!isdigit(*buf)) {
    send_to_char("Usage: oload [vnum]\r\n", ch);
    return;
  }

  if ((number = atoi(buf)) < 0) {
    send_to_char("A NEGATIVE number??\r\n", ch);
    return;
  }

  if ((r_num = real_object(number)) < 0) {
    send_to_char("There is no object with that number.\r\n", ch);
    return;
  }
  obj = read_object(r_num, REAL);
  sprintf(buf, "Poof!  You create %s.\r\n", obj->short_description);
    sprintf(buf2, "load: obj #%i oloaded by %s", GET_OBJ_VNUM(obj),
      GET_NAME(ch));
    log(buf2);
  send_to_char(buf, ch);
  obj_to_char(obj, ch);
}



/* save object edit buffer to obj_proto and *.obj file
*/
ACMD(do_osave)
{
  sh_int i, obj_new_vnum;

  if(!OBJ_NEW(ch)) {
    send_to_char("There is no object in the edit buffer.  Use oedit to set one.\r\n", ch);
    return;
  } 

  obj_new_vnum = GET_OBJ_VNUM(OBJ_NEW(ch));
  i = real_object(obj_new_vnum);

  if(copy_object(obj_proto + i, OBJ_NEW(ch))) {
    send_to_char("Error: unable to copy object to prototype\r\n", ch);
    return;
  }

  fix_obj_strings(obj_proto + i, OBJ_NEW(ch));

  i = get_zon_num(obj_new_vnum);

  if(objs_to_file(i)) {
    send_to_char("Error:  could not write object file.\r\n", ch);
    return;
  }

  sprintf(buf2, "olc: osave  : #%i saved in obj file by %s", obj_new_vnum,
    GET_NAME(ch));
  mudlog(buf2, CMP, LVL_GOD, TRUE);
  send_to_char("Object prototype saved in obj file.\r\n", ch);
}


/* save mob edit buffer to mob_proto and *.mob file
*/
void save_mob(struct char_data * ch)
{
  int i, mob_new_vnum;

  if(!MOB_NEW(ch)) {
    send_to_char("There is no mob in the edit buffer.  Use medit to set one.\r\n", ch);
    return;
  } 

  mob_new_vnum = GET_MOB_VNUM(MOB_NEW(ch));
  i = real_mobile(mob_new_vnum);

  if(copy_mobile(mob_proto + i, MOB_NEW(ch))) {
    send_to_char("Error:  could not copy mob to prototype\r\n", ch);
    return;
  }

  fix_mob_strings(mob_proto + i, MOB_NEW(ch));

  i = get_zon_num(mob_new_vnum);

  if(mobs_to_file(i)) {
    send_to_char("Error:  could not write mob file.\r\n", ch);
    return;
  }    
  sprintf(buf2, "olc: msave  : #%i saved in mob file by %s", mob_new_vnum,
    GET_NAME(ch));
  mudlog(buf2, CMP, LVL_GOD, TRUE);
  send_to_char("Mob prototype saved in mob file.\r\n", ch);
}


/* set the object to be edited */
ACMD(do_oedit)
{
  int number, r_num, i = 0, j, zone, zn, top_obj;
  int obj_new_num = NOTHING, obj_new_new = FALSE;
  char buf[1024], arg[MAX_INPUT_LENGTH];

  half_chop(argument, buf, arg);

  if(GET_LEVEL(ch) >= LVL_IMPL && is_abbrev(buf, "allow")) {
    obj_allow = TRUE;
    sprintf(buf, "olc: %s turns obj editor access ON", GET_NAME(ch));
    mudlog(buf, BRF, LVL_GOD, TRUE);
    return;
  }
  if(GET_LEVEL(ch) >= LVL_IMPL && is_abbrev(buf, "deny")) {
    obj_allow = FALSE;
    sprintf(buf, "olc: %s turns obj editor access OFF", GET_NAME(ch));
    mudlog(buf, BRF, LVL_GOD, TRUE);
    return;
  }

  if(!obj_allow) {
    send_to_char("The object editor is currently disabled.\r\n", ch);
    return;
  }

  if(is_abbrev(buf, "default")) {
    if(!*arg) {
      send_to_char("Usage   : oedit default <object vnum>\r\n", ch);
      return;
    }
    if ((number = atoi(arg)) < 0) {
      send_to_char("A NEGATIVE number??\r\n", ch);
      return;
    }
    if ((r_num = real_object(number)) < 0) {
      send_to_char("There is no object with that number.\r\n", ch);
      return;
    }
    DEF_OBJ(ch) = r_num;
    sprintf(buf, "Changing default object to #%d - %s.\r\n",
      number, obj_proto[r_num].short_description);
    send_to_char(buf, ch);
    return;
  }

  if(!*buf || (!isdigit(*buf) && !is_abbrev(buf, "create")
    && !is_abbrev(buf, "done"))) {
    send_to_char("Usage: oedit (<vnum>|create|done|default <vnum>)\r\n", ch);
    return;
  }

  if(is_abbrev(buf, "done")) {
    if(!OBJ_NEW(ch)) {
      send_to_char("You're not editing an object.\r\n", ch);
      return;
    }
    free_obj(OBJ_NEW(ch));
    OBJ_NEW(ch) = NULL;
    send_to_char("Exiting object editor.\r\n", ch);
    return;
  }

  if(is_abbrev(buf, "delete")) {
    if(GET_LEVEL(ch) < LVL_IMPL) {
      send_to_char("You're not studly enough to do that.\r\n", ch);
      return;
    }
    if(!*arg) {
      send_to_char("Usage   : oedit delete <obj vnum>\r\n", ch);
      return;
    }
    if ((number = atoi(arg)) < 0) {
      send_to_char("A NEGATIVE number??\r\n", ch);
      return;
    }
    if ((r_num = real_object(number)) < 0) {
      send_to_char("There is no object with that number.\r\n", ch);
      return;
    }
    obj_index[r_num].deleted = TRUE;
    objs_to_file(get_zon_num(number));
    sprintf(buf, "OLC: %s deleted obj #%d, %s.\r\n", GET_NAME(ch), number,
      obj_proto[r_num].short_description);
    mudlog(buf, BRF, LVL_GOD, TRUE);
    return;
  }

  if(is_abbrev(buf, "undelete")) {
    if(GET_LEVEL(ch) < LVL_IMPL) {
      send_to_char("You're not studly enough to do that.\r\n", ch);
      return;
    }
    if(!*arg) {
      send_to_char("Usage   : oedit undelete <obj vnum>\r\n", ch);
      return;
    }
    if ((number = atoi(arg)) < 0) {
      send_to_char("A NEGATIVE number??\r\n", ch);
      return;
    }
    if ((r_num = real_object(number)) < 0) {
      send_to_char("There is no object with that number.\r\n", ch);
      return;
    }
    obj_index[r_num].deleted = FALSE;
    objs_to_file(get_zon_num(number));
    sprintf(buf, "OLC: %s undeleted obj #%d, %s.\r\n", GET_NAME(ch), number,
      obj_proto[r_num].short_description);
    mudlog(buf, BRF, LVL_GOD, TRUE);
    return;
  }

  if(is_abbrev(buf, "create")) {
    zn = get_zon_num(world[ch->in_room].number);
    zone = get_zone_perms(ch, zn);
    if(zone == -1) return;
    obj_new_new = TRUE;
    top_obj = get_top(zn);
    for(j = zone * 100; j <= top_obj; j++) {
      if(j > top_obj) {
	send_to_char("No free object vnums in this zone.\r\n", ch);
	return;
      }
      if (real_object(j) < 0) {
	obj_new_num = j;
	obj_new_new = TRUE;
	sprintf(buf, "New object #%i in zone #%i.\r\n", j, zone);
	send_to_char(buf, ch);
	break;
      } 
    }
  } else {
    if ((number = atoi(buf)) < 0) {
      send_to_char("A NEGATIVE number??\r\n", ch);
      return;
    }

    if ((r_num = real_object(number)) < 0) {
      send_to_char("There is no object with that number.\r\n", ch);
      return;
    }

    if(OBJ_NEW(ch) && GET_OBJ_RNUM(OBJ_NEW(ch)) == r_num) {
      send_to_char("You are already editing that object.\r\n", ch);
      return;
    }

    zn = get_zon_num(number);
    zone = get_zone_perms(ch, zn);
    if(zone == -1) return;

    if(!check_edit_obj(ch, number)) {
      OBJ_NEW(ch) = NULL;
      return;
    }

    i = r_num;
  
  }
  if (OBJ_NEW(ch)) free_obj(OBJ_NEW(ch));
  CREATE(OBJ_NEW(ch), struct obj_data, 1);
  if(obj_new_new) {
    if(new_objects == max_new_objects) {
      send_to_char("There are no more new object slots.  Reboot for more.\r\n", ch);
      free_obj(OBJ_NEW(ch));
      return;
    }
    top_of_objt++;
    new_objects++;
    i = top_of_objt;
    obj_index[i].vnum = obj_new_num;
    obj_index[i].number = 0;
    obj_index[i].func = NULL;
    obj_proto[i].in_room = NOWHERE;
    obj_proto[i].item_number = top_of_objt;
    obj_index[i].deleted = FALSE;

    if(DEF_OBJ(ch) >= 0)
      copy_object(obj_proto + i, obj_proto + DEF_OBJ(ch));
    else {

      obj_proto[i].name = str_dup("object new");
      obj_proto[i].short_description = str_dup("a new object");
      obj_proto[i].description = str_dup("A new object is here.");
      obj_proto[i].action_description = NULL;
      obj_proto[i].ex_description = NULL;
    }
  }

  copy_object(OBJ_NEW(ch), obj_proto + i);
  OBJ_NEW(ch)->item_number = i;

  sprintf(buf, "You begin editing %s.\r\n", obj_proto[i].short_description);
  send_to_char(buf, ch);
}



/* set the mob to be edited */
ACMD(do_medit)
{
  int number, r_num, i = 0, j, k, zone, zn, top_mob;
  int mob_new_num = NOTHING, mob_new_new = FALSE;
  char buf[1024], arg[MAX_INPUT_LENGTH];

  half_chop(argument, buf, arg);

  if(GET_LEVEL(ch) >= LVL_IMPL && is_abbrev(buf, "allow")) {
    mob_allow = TRUE;
    sprintf(buf, "olc: %s turns mob editor access ON", GET_NAME(ch));
    mudlog(buf, BRF, LVL_GOD, TRUE);
    return;
  }
  if(GET_LEVEL(ch) >= LVL_IMPL && is_abbrev(buf, "deny")) {
    mob_allow = FALSE;
    sprintf(buf, "olc: %s turns mob editor access OFF", GET_NAME(ch));
    mudlog(buf, BRF, LVL_GOD, TRUE);
    return;
  }

  if(!mob_allow) {
    send_to_char("The mob editor is currently disabled.\r\n", ch);
    return;
  }

  if(is_abbrev(buf, "default")) {
    if(!*arg) {
      send_to_char("Usage   : medit default <mob vnum>\r\n", ch);
      return;
    }
    if ((number = atoi(arg)) < 0) {
      send_to_char("A NEGATIVE number??\r\n", ch);
      return;
    }
    if ((r_num = real_mobile(number)) < 0) {
      send_to_char("There is no mob with that number.\r\n", ch);
      return;
    }
    DEF_MOB(ch) = r_num;
    sprintf(buf, "Changing default mob to #%d - %s.\r\n",
      number, mob_proto[r_num].player.short_descr);
    send_to_char(buf, ch);
    return;
  }

  if(!*buf || (!isdigit(*buf) && !is_abbrev(buf, "create")
    && !is_abbrev(buf, "done") && !is_abbrev(buf, "save"))) {
    send_to_char("Usage: medit (<vnum>|create|save|done|default <vnum>)\r\n", ch);
    return;
  }

  if(is_abbrev(buf, "save")) {
    save_mob(ch);
    return;
  }

  if(is_abbrev(buf, "done")) {
    if(!MOB_NEW(ch)) {
      send_to_char("You're not editing a mob.\r\n", ch);
      return;
    }
    free_char(MOB_NEW(ch));
    MOB_NEW(ch) = NULL;
    send_to_char("Exiting mob editor.\r\n", ch);
    return;
  }

  if(is_abbrev(buf, "delete")) {
    if(GET_LEVEL(ch) < LVL_IMPL) {
      send_to_char("You're not studly enough to do that.\r\n", ch);
      return;
    }
    if(!*arg) {
      send_to_char("Usage   : medit delete <mob vnum>\r\n", ch);
      return;
    }
    if ((number = atoi(arg)) < 0) {
      send_to_char("A NEGATIVE number??\r\n", ch);
      return;
    }
    if ((r_num = real_mobile(number)) < 0) {
      send_to_char("There is no mob with that number.\r\n", ch);
      return;
    }
    mob_index[r_num].deleted = TRUE;
    mobs_to_file(get_zon_num(number));
    sprintf(buf, "OLC: %s deleted mob #%d, %s.\r\n", GET_NAME(ch), number,
      mob_proto[r_num].player.short_descr);
    mudlog(buf, BRF, LVL_GOD, TRUE);
    return;
  }

  if(is_abbrev(buf, "undelete")) {
    if(GET_LEVEL(ch) < LVL_IMPL) {
      send_to_char("You're not studly enough to do that.\r\n", ch);
      return;
    }
    if(!*arg) {
      send_to_char("Usage   : medit undelete <mob vnum>\r\n", ch);
      return;
    }
    if ((number = atoi(arg)) < 0) {
      send_to_char("A NEGATIVE number??\r\n", ch);
      return;
    }
    if ((r_num = real_mobile(number)) < 0) {
      send_to_char("There is no mob with that number.\r\n", ch);
      return;
    }
    mob_index[r_num].deleted = FALSE;
    mobs_to_file(get_zon_num(number));
    sprintf(buf, "OLC: %s undeleted mob #%d, %s.\r\n", GET_NAME(ch), number,
      mob_proto[r_num].player.short_descr);
    mudlog(buf, BRF, LVL_GOD, TRUE);
    return;
  }

  if(is_abbrev(buf, "create")) {
    zn = get_zon_num(world[ch->in_room].number);
    zone = get_zone_perms(ch, zn);
    if(zone == -1) return;
    mob_new_new = TRUE;
    top_mob = get_top(zn);
    for(j = zone * 100; j <= top_mob; j++) {
      if(j > top_mob) {
	send_to_char("No free mob vnums in this zone.\r\n", ch);
	return;
      }
      if (real_mobile(j) < 0) {
	mob_new_num = j;
	mob_new_new = TRUE;
	sprintf(buf, "New mob #%i in zone #%i.\r\n", j, zone);
	send_to_char(buf, ch);
	break;
      } 
    }
  } else {
    if ((number = atoi(buf)) < 0) {
      send_to_char("A NEGATIVE number??\r\n", ch);
      return;
    }

    if ((r_num = real_mobile(number)) < 0) {
      send_to_char("There is no mob with that number.\r\n", ch);
      return;
    }

    if(MOB_NEW(ch) && GET_MOB_RNUM(MOB_NEW(ch)) == r_num) {
      send_to_char("You are already editing that mob.\r\n", ch);
      return;
    }

    zn = get_zon_num(number);
    zone = get_zone_perms(ch, zn);
    if(zone == -1) return;

    if(!check_edit_mob(ch, number)) {
      MOB_NEW(ch) = NULL;
      return;
    }
  
    i = r_num;

  }
  if (MOB_NEW(ch)) free_char(MOB_NEW(ch));
  CREATE(MOB_NEW(ch), struct char_data, 1);
  if(mob_new_new) {
    if(new_mobs == max_new_mobs) {
      send_to_char("There are no more new mob slots.  Reboot for more.\r\n", ch);
      free_char(MOB_NEW(ch));
      return;
    }
    top_of_mobt++;
    new_mobs++;
    i = top_of_mobt;
    mob_index[i].vnum = mob_new_num;
    mob_index[i].number = 0;
    mob_index[i].func = NULL;
    mob_proto[i].nr = i;
    mob_proto[i].desc = NULL;
    mob_proto[i].player_specials = &dummy_mob;
    mob_proto[i].in_room = NOWHERE;
    mob_index[i].deleted = FALSE;

    if(DEF_MOB(ch) >= 0)
      copy_mobile(mob_proto + i, mob_proto + DEF_MOB(ch));
    else {

      mob_proto[i].player.name = str_dup("mob new");
      mob_proto[i].player.short_descr = str_dup("A new mob");
      mob_proto[i].player.long_descr = str_dup("A new mob is existing here.\n");
      mob_proto[i].player.description = NULL;
      mob_proto[i].player.title = NULL;
      MOB_FLAGS(mob_proto + i) = 0;
      SET_BIT(MOB_FLAGS(mob_proto + i), MOB_ISNPC);
      AFF_FLAGS(mob_proto + i) = 0;
      GET_ALIGNMENT(mob_proto + i) = 0;
      mob_proto[i].real_abils.str = 11;
      mob_proto[i].real_abils.intel = 11;
      mob_proto[i].real_abils.wis = 11;
      mob_proto[i].real_abils.dex = 11;
      mob_proto[i].real_abils.con = 11;
      mob_proto[i].real_abils.cha = 11;
      GET_LEVEL(mob_proto + i) = 1;
      mob_proto[i].points.hitroll = 0;
      mob_proto[i].points.armor = 100;
      mob_proto[i].points.max_hit = 0;
      mob_proto[i].points.hit = 1;
      mob_proto[i].points.mana = 3;
      mob_proto[i].points.move = 10;
      mob_proto[i].points.max_mana = 10;
      mob_proto[i].points.max_move = 50;
      mob_proto[i].mob_specials.damnodice = 1;
      mob_proto[i].mob_specials.damsizedice = 3;
      mob_proto[i].points.damroll = 0;
      GET_GOLD(mob_proto + i) = 0;
      GET_EXP(mob_proto + i) = 0;
      mob_proto[i].mob_specials.attack_type = 0;
      mob_proto[i].char_specials.position = 8;
      mob_proto[i].mob_specials.default_pos = 8;
      mob_proto[i].player.sex = 0;
      mob_proto[i].player.class = 0;
      mob_proto[i].player.weight = 200;
      mob_proto[i].player.height = 198;
      for(k = 0; k < 3; k++)
        GET_COND(mob_proto + i, k) = -1;
      for(k = 0; k < 5; k++)
        GET_SAVE(mob_proto + i, k) = 0;
      mob_proto[i].aff_abils = mob_proto[i].real_abils;
    }
  } 

  copy_mobile(MOB_NEW(ch), mob_proto + i);
  MOB_NEW(ch)->nr = i;

  sprintf(buf, "You begin editing %s.\r\n", mob_proto[i].player.short_descr);
  send_to_char(buf, ch);
}


ACMD(do_zallow)
{
  int zone, rnum = -1;
  FILE *old_file, *new_file;
  char *old_fname, *new_fname, line[256];
  char arg1[40], arg2[40];
  struct builder_list *bldr;

  half_chop(argument, arg1, arg2);

  if(!*arg1 || !*arg2 || !isdigit(*arg2)) {
    send_to_char("Usage  : zallow <player> <zone>\r\n", ch);
    return;
  }
  *arg1 = UPPER(*arg1);
  zone = atoi(arg2);
  if((rnum = get_zone_rnum(zone)) == -1) {
    send_to_char("That zone doesn't seem to exist.\r\n", ch);
    return;
  }

  if(zone_table[rnum].builders == NULL)
    read_zone_perms(rnum);

  for(bldr = zone_table[rnum].builders; bldr; bldr = bldr->next) {
    if(!str_cmp(bldr->name, arg1)) {
      send_to_char("That person already has access to that zone.\r\n", ch);
      return;
    }
  }

  CREATE(bldr, struct builder_list, 1);
  bldr->name = str_dup(arg1);
  bldr->next = zone_table[rnum].builders;
  zone_table[rnum].builders = bldr;

  sprintf(buf1, "%s/%i.zon", ZON_PREFIX, zone);
  old_fname = buf1;
  sprintf(buf2, "%s/%i.zon.temp", ZON_PREFIX, zone);
  new_fname = buf2;

  if(!(old_file = fopen(old_fname, "r"))) {
    sprintf(buf, "Error: Could not open %s for read.\r\n", old_fname);
    send_to_char(buf, ch);
    return;
  }
  if(!(new_file = fopen(new_fname, "w"))) {
    sprintf(buf, "Error: Could not open %s for write.\r\n", new_fname);
    send_to_char(buf, ch);
    fclose(old_file);
    return;
  }
  get_line2(old_file, line);
  fprintf(new_file, "%s\n", line);
  get_line2(old_file, line);
  fprintf(new_file, "%s\n", line);
  get_line2(old_file, line);
  fprintf(new_file, "* Builder  %s\n", arg1);
  fprintf(new_file, "%s\n", line);
  do {
    get_line2(old_file, line);
    fprintf(new_file, "%s\n", line);
  } while(*line != '$');
  fclose(old_file);
  fclose(new_file);
  remove(old_fname);
  rename(new_fname, old_fname);
  send_to_char("Zone file edited.\r\n", ch);
  sprintf(buf, "olc: %s gives %s zone #%i builder access.", GET_NAME(ch),
    arg1, zone);
  mudlog(buf, BRF, LVL_IMPL, TRUE);
}


ACMD(do_zdeny)
{
  int zone, rnum, found = FALSE;
  FILE *old_file, *new_file;
  char *old_fname, *new_fname, line[256];
  char arg1[40], arg2[40], arg3[40], arg4[40], arg5[40], arg6[40];
  struct builder_list *bldr, *lbldr;

  half_chop(argument, arg1, arg2);

  if(!*arg1 || !*arg2 || !isdigit(*arg2)) {
    send_to_char("Usage  : zdeny <player> <zone>\r\n", ch);
    return;
  }
  *arg1 = UPPER(*arg1);
  zone = atoi(arg2);
  if((rnum = get_zone_rnum(zone)) == -1) {
    send_to_char("That zone doesn't seem to exist.\r\n", ch);
    return;
  }

  if(zone_table[rnum].builders == NULL)
    read_zone_perms(rnum);

  lbldr = NULL;

  for(bldr = zone_table[rnum].builders; bldr; ) {
    if(!str_cmp(bldr->name, arg1)) {
      if(!lbldr) {
	zone_table[rnum].builders = bldr->next;
	free(bldr->name);
	free(bldr);
	bldr = zone_table[rnum].builders;
      }  else {
	lbldr->next = bldr->next;
	free(bldr->name);
	free(bldr);
	bldr = lbldr->next;
      }

      found = TRUE;
    } else {
      lbldr = bldr;
      bldr = bldr->next;
    }
  }

  if(!found) {
    send_to_char("That person doesn't have access to that zone.\r\n", ch);
    return;
  }

  if(zone_table[rnum].builders == NULL) {
    CREATE(zone_table[rnum].builders, struct builder_list, 1);
    zone_table[rnum].builders->name = str_dup("*");
    zone_table[rnum].builders->next = NULL;
  }

  sprintf(buf1, "%s/%i.zon", ZON_PREFIX, zone);
  old_fname = buf1;
  sprintf(buf2, "%s/%i.zon.temp", ZON_PREFIX, zone);
  new_fname = buf2;

  if(!(old_file = fopen(old_fname, "r"))) {
    sprintf(buf, "Error: Could not open %s for read.\r\n", old_fname);
    send_to_char(buf, ch);
    return;
  }
  if(!(new_file = fopen(new_fname, "w"))) {
    sprintf(buf, "Error: Could not open %s for write.\r\n", new_fname);
    send_to_char(buf, ch);
    fclose(old_file);
    return;
  }
  do {
    get_line2(old_file, line);
    if(*line == '*') {
      half_chop(line, arg3, arg4);
      half_chop(arg4, arg5, arg6);
      if(!*arg5 || !strcmp(arg5, "Builder")) {
	fprintf(new_file, "%s\n", line);
      } else if(str_cmp(arg1, arg6))
	       fprintf(new_file, "%s\n", line);
    } else {
      fprintf(new_file, "%s\n", line);
    }
  } while(*line != '$');
  fclose(old_file);
  fclose(new_file);
  remove(old_fname);
  rename(new_fname, old_fname);
  send_to_char("Zone file edited.\r\n", ch);
  sprintf(buf, "olc: %s removes %s's zone #%i builder access.", GET_NAME(ch),
    arg1, zone);
  mudlog(buf, BRF, LVL_IMPL, TRUE);
}


/* the room editor has been moved to do_redit */
ACMD(do_zedit)
{
  int zone, i, zn;
  char field[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH];
  char *fname;
  FILE *tempfile;

  half_chop(argument, field, val_arg);

  if(is_abbrev(field, "allow")) {
    if(GET_LEVEL(ch) < LVL_IMPL) 
      return;
    zone_allow = TRUE;
    sprintf(buf, "olc: %s turns zone editor access ON", GET_NAME(ch));
    mudlog(buf, BRF, LVL_GOD, TRUE);
    return;
  }
  if(is_abbrev(field, "deny")) {
    if(GET_LEVEL(ch) < LVL_IMPL) 
      return;
    zone_allow = FALSE;
    sprintf(buf, "olc: %s turns zone editor access OFF", GET_NAME(ch));
    mudlog(buf, BRF, LVL_GOD, TRUE);
    return;
  }

  if(!zone_allow) {
    send_to_char("The zone editor is currently disabled.\r\n", ch);
    return;
  }

  if(is_abbrev(field, "save")) {
    if(ZONE_BUF(ch) == NOWHERE) {
      send_to_char("No zone is being edited.\r\n", ch);
      return;
    } else {
      send_to_char("Zone saved.\r\n", ch);
      save_zone(ch, ZONE_BUF(ch));
      sprintf(buf, "olc: %s saves zone #%i", GET_NAME(ch), ZONE_BUF(ch));
      mudlog(buf, BRF, LVL_GOD, TRUE);
      return;
    }
  }

  if(is_abbrev(field, "done")) {
    ZONE_BUF(ch) = NOWHERE;
    send_to_char("Exiting the zone editor.\r\n", ch);
    return;
  }

  if(is_abbrev(field, "create")) {
    if(GET_LEVEL(ch) < LVL_IMPL) {
      send_to_char("You're not holy enough to conjure a zone.\r\n", ch);
      return;
    }
    if(!*val_arg || !isdigit(*val_arg)) {
      send_to_char("Please specify a zone number to create.\r\n", ch);
      return;
    }

    zone = atoi(val_arg);
    if(zone > 326) {
      send_to_char("326 is the highest zone you can conjure.\r\n", ch);
      return;
    }
    sprintf(buf1, "%s/%i.zon", ZON_PREFIX, zone);
    fname = buf1;

    if((tempfile = fopen(fname, "r"))) {
      fclose(tempfile);
      send_to_char("That zone already exists.\r\n", ch);
      return;
    }
    if(!(tempfile = fopen(fname, "w"))) {
      send_to_char("Error:  can't write new zone file\r\n", ch);
      return;
    }
    fprintf(tempfile, "#%i\n", zone);
    fprintf(tempfile, "Zone %i~\n", zone);
    fprintf(tempfile, "%i 25 0\n", (zone * 100) + 99);
    fprintf(tempfile, "S\n");
    fprintf(tempfile, "$\n");
    fclose(tempfile);
    send_to_char("Wrote new zone file\r\n", ch);

    if(!add_to_index(zone, "zon")) {
      send_to_char("Error editing zone index\r\n", ch);
      return;
    }
    send_to_char("Added zone to zone index\r\n", ch);

    zone_table = add_zone(zone_table, zone);
    send_to_char("Added zone to memory\r\n", ch);

    sprintf(buf1, "%s/%i.wld", WLD_PREFIX, zone);
    fname = buf1;

    if(!(tempfile = fopen(fname, "w"))) {
      send_to_char("Error:  can't write new world file\r\n", ch);
      return;
    }
    fprintf(tempfile, "#%i\n",zone * 100);
    fprintf(tempfile, "the start of zone %i~\n",zone);
    fprintf(tempfile, "You are in a room.\n~\n");
    fprintf(tempfile, "%i 0 0\n", zone);
    fprintf(tempfile, "S\n");
    fprintf(tempfile, "$\n");
    fclose(tempfile);
    send_to_char("Wrote new world file\r\n", ch);

    if(new_rooms == max_new_rooms)
      send_to_char("Reached maximum number of rooms.  Reboot for more.\r\n", ch);
    else {
      top_of_world++;
      new_rooms++;
      world[top_of_world].zone = zone;
      world[top_of_world].number = zone * 100;
      world[top_of_world].name = str_dup("The start of the zone");
      world[top_of_world].description = str_dup("You are in a room.\r\n");
      world[top_of_world].room_flags = 0;
      world[top_of_world].sector_type = 0;
      world[top_of_world].func = NULL;
      world[top_of_world].contents = NULL;
      world[top_of_world].people = NULL;
      world[top_of_world].light = 0;
      world[top_of_world].ex_description = NULL;

      for(i = 0; i < NUM_OF_DIRS; i++)
	world[top_of_world].dir_option[i] = NULL;

      sprintf(buf, "Created new room #%i\r\n", zone * 100);
      send_to_char(buf, ch);
    }

    if(!add_to_index(zone, "wld")) {
      send_to_char("Error editing world index\r\n", ch);
      return;
    }
    send_to_char("Added zone to world index\r\n", ch);

    sprintf(buf1, "%s/%i.mob", MOB_PREFIX, zone);
    fname = buf1;

    if(!(tempfile = fopen(fname, "w"))) {
      send_to_char("Error:  can't write new mob file\r\n", ch);
      return;
    }
    fprintf(tempfile, "$\n");
    fclose(tempfile);
    send_to_char("Wrote new mob file\r\n", ch);

    if(!add_to_index(zone, "mob")) {
      send_to_char("Error editing mob index\r\n", ch);
      return;
    }
    send_to_char("Added zone to mob index\r\n", ch);

    sprintf(buf1, "%s/%i.obj", OBJ_PREFIX, zone);
    fname = buf1;

    if(!(tempfile = fopen(fname, "w"))) {
      send_to_char("Error:  can't write new object file\r\n", ch);
      return;
    }
    fprintf(tempfile, "$\n");
    fclose(tempfile);
    send_to_char("Wrote new object file\r\n", ch);

    if(!add_to_index(zone, "obj")) {
      send_to_char("Error editing object index\r\n", ch);
      return;
    }
    send_to_char("Added zone to object index\r\n", ch);

    save_rooms(zone);
    send_to_char("Zone created\r\n", ch);
    sprintf(buf, "olc: %s creates new zone #%i", GET_NAME(ch), zone);
    mudlog(buf, BRF, LVL_GOD, TRUE);
    return;
  }

  zn = get_zon_num(world[ch->in_room].number);

  if(ZONE_BUF(ch) == zn) {
    send_to_char("You are already editing this zone.\r\n", ch);
    return;
  }

  zone = get_zone_perms(ch, zn);
  if(zone == -1)
    return;
  if(!check_edit_zone(ch, zone)) {
    ZONE_BUF(ch) = NOWHERE;
    return;
  }

  ZONE_BUF(ch) = zone;

  sprintf(buf, "olc: %s begins editing zone #%i.", GET_NAME(ch), zone);
  mudlog(buf, CMP, LVL_GOD, TRUE);
  sprintf(buf, "You begin editing zone #%i.\r\n", zone);
  send_to_char(buf, ch);
}


/* this is mostly code that used to be in do_zedit */
ACMD(do_redit)
{
  int zone, zn, number, r_num;
  char field[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH];

  half_chop(argument, field, val_arg);

  if(is_abbrev(field, "allow")) {
    if(GET_LEVEL(ch) < LVL_IMPL) 
      return;
    room_allow = TRUE;
    sprintf(buf, "olc: %s turns room editor access ON", GET_NAME(ch));
    mudlog(buf, BRF, LVL_GOD, TRUE);
    return;
  }
  if(is_abbrev(field, "deny")) {
    if(GET_LEVEL(ch) < LVL_IMPL) 
      return;
    room_allow = FALSE;
    sprintf(buf, "olc: %s turns room editor access OFF", GET_NAME(ch));
    mudlog(buf, BRF, LVL_GOD, TRUE);
    return;
  }

  if(is_abbrev(field, "default")) {
    if(!*val_arg) {
      send_to_char("Usage   : redit default <room vnum>\r\n", ch);
      return;
    }
    if ((number = atoi(val_arg)) < 0) {
      send_to_char("A NEGATIVE number??\r\n", ch);
      return;
    }
    if ((r_num = real_room(number)) < 0) {
      send_to_char("There is no room with that number.\r\n", ch);
      return;
    }
    DEF_ROOM(ch) = r_num;
  }

  if(!room_allow) {
    send_to_char("The room editor is currently disabled.\r\n", ch);
    return;
  }

  if(is_abbrev(field, "save")) {
    if(ROOM_BUF(ch) == NOWHERE) {
      send_to_char("No zone is being edited.\r\n", ch);
      return;
    } else {
      save_rooms(ROOM_BUF(ch));
      send_to_char("Rooms saved.\r\n", ch);
      sprintf(buf, "olc: %s saves zone #%i rooms", GET_NAME(ch), ROOM_BUF(ch));
      mudlog(buf, BRF, LVL_GOD, TRUE);
      return;
    }
  }

  if(is_abbrev(field, "done")) {
    ROOM_BUF(ch) = NOWHERE;
    return;
  }

  zn = get_zon_num(world[ch->in_room].number);

  if(ROOM_BUF(ch) == zn) {
    send_to_char("You are already editing the rooms in this zone.\r\n", ch);
    return;
  }

  zone = get_zone_perms(ch, zn);
  if(zone == -1)
    return;
  if(!check_edit_zrooms(ch, zone)) {
    ROOM_BUF(ch) = NOWHERE;
    return;
  }

  ROOM_BUF(ch) = zone;

  sprintf(buf, "olc: %s begins editing zone #%i rooms.", GET_NAME(ch), zone);
  mudlog(buf, CMP, LVL_GOD, TRUE);
  sprintf(buf, "You begin editing zone #%i rooms.\r\n", zone);
  send_to_char(buf, ch);
}



ACMD(do_oset)
{
  int i, afftype, affmod, cmmd = 0, length = 0, help = FALSE;
  char field[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH];
  char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
  struct extra_descr_data *desc = NULL, *last = NULL;
  int neg_affect = FALSE;

  char *fields[] = 
    {   "name"      ,"short"      ,"long"        ,"type"       ,/*0-3*/
	"extra"     ,"wear"       ,"v0"          ,"v1"         ,
	"v2"        ,"v3"         ,"weight"      ,"cost"       ,
	"rent"      ,"light"      ,"scroll"      ,"wand"       ,/*12-15*/
	"staff"     ,"weapon"     ,"treasure"    ,"armor"      ,
	"potion"    ,"worn"       ,"other"       ,"trash"      ,
	"container" ,"note"       ,"drinkcon"    ,"key"        ,/*24-27*/
	"food"      ,"money"      ,"pen"         ,"boat"       ,
	"fountain"  ,"glow"       ,"hum"         ,"norent"     ,
	"nodonate"  ,"noinvis"    ,"invisible"   ,"magic"      ,/*36-39*/
	"nodrop"    ,"bless"      ,"nogood"      ,"noevil"     ,
	"noneutral" ,"nomage"     ,"nocleric"    ,"nothief"    ,
	"nowarrior" ,"nosell"     ,"take"        ,"finger"     ,/*48-51*/
	"neck"      ,"body"       ,"head"        ,"legs"       ,
	"feet"      ,"hands"      ,"arms"        ,"shield"     ,
	"about"     ,"waist"      ,"wrist"       ,"wield"      ,/*60-63*/
	"hold"      ,"action"     ,"reserved"    ,"reserved"   ,
	"reserved"  ,"reserved"   ,"reserved"    ,"reserved"   ,
	"reserved"  ,"reserved"   ,"reserved"    ,"affect"     ,
	"edesc"     ,"reserved"   ,"\n" };
  char *affs[] =
    {   "none"      ,"str"        ,"dex"         ,"int"        ,
	"wis"       ,"con"        ,"cha"         ,"age"        ,
	"weight"    ,"height"     ,"mana"        ,"hit"        ,
	"move"      ,"ac"         ,"hitroll"     ,"damroll"    ,
	"paralysis" ,"rod"        ,"petrify"     ,"breath"     ,
	"spell"     ,"\n" };

  half_chop(argument, field, val_arg);

  if(!OBJ_NEW(ch)) {
    send_to_char("There is no object in the edit buffer.  Use oedit to set one.\r\n\r\n", ch);
    *val_arg = '*';
    return;
  }

  if (!*field) *field = '*';

  do {
  for (length = strlen(field), cmmd = 0; *fields[cmmd] != '\n'; cmmd++) 
    if (!strn_cmp(fields[cmmd], field, length))  
      break;

  switch (cmmd) {
  case 0:
    if(!*val_arg) {
      send_to_char("Usage   :   oset name <alias list>\r\n\r\n", ch);
      send_to_char("<alias list> should be a list of names players can use to act on the\r\n", ch);
      send_to_char("object.\r\n\r\n", ch);
      send_to_char("Example :   oset name sword short\r\n", ch);
      break;
    }
    OBJ_NEW(ch)->name = str_dup(val_arg);
    *val_arg = '\0';
    break;
  case 1:
    if(!*val_arg) {
      send_to_char("Usage   :  oset short <object short description>\r\n", ch);
      send_to_char("Example :  oset short a short sword\r\n", ch);
      break;
    }
    OBJ_NEW(ch)->short_description = str_dup(val_arg);
    *val_arg = '\0';
    break;
  case 2:
    if(!*val_arg) {
      send_to_char("Usage   :  oset long <object long description>\r\n", ch);
      send_to_char("An object's long description is the text you see when it is lying on\r\n", ch);
      send_to_char("the ground.  The first word should be capitalized, and it should end\r\n", ch);
      send_to_char("with a period.\r\n\r\n", ch);
      send_to_char("Example :  oset long A short sword is lying here.\r\n", ch);
      break;
    }
    OBJ_NEW(ch)->description = str_dup(val_arg);
    *val_arg = '\0';
    break;
  case 3:
	send_to_char("Usage   :  oset <object type>\r\n\r\n", ch);
	send_to_char("Valid object types are:\r\n", ch);
	send_to_char("   light             note\r\n", ch);
	send_to_char("   scroll            drinkcon\r\n", ch);
	send_to_char("   wand              key\r\n", ch);
	send_to_char("   staff             food\r\n", ch);
	send_to_char("   weapon            money\r\n", ch);
	send_to_char("   treasure          pen\r\n", ch);
	send_to_char("   armor             boat\r\n", ch);
	send_to_char("   potion            fountain\r\n", ch);
	send_to_char("   worn              \r\n", ch);
	send_to_char("   other             \r\n", ch);
	send_to_char("   trash             \r\n", ch);
	send_to_char("   container\r\n\r\n", ch);
	send_to_char("Example :  oset weapon\r\n", ch);
	send_to_char("           oset potion\r\n", ch);
	break;
  case 4:
	send_to_char("Usage   :  oset <extra flag>\r\n\r\n", ch);
	send_to_char("This will toggle the flag you specify.\r\n", ch);
	send_to_char("Valid extra flags are:\r\n", ch);
	send_to_char("   glow              nogood\r\n", ch);
	send_to_char("   hum               noevil\r\n", ch);
	send_to_char("   norent            noneutral\r\n", ch);
	send_to_char("   nodonate          nomage\r\n", ch);
	send_to_char("   noinvis           nocleric\r\n", ch);
	send_to_char("   invisible         nothief\r\n", ch);
	send_to_char("   magic	           nowarrior\r\n", ch);
	send_to_char("   nodrop            nosell\r\n", ch);
	send_to_char("   nopaladin         noapaladin\r\n", ch);
	send_to_char("   nomort            nosmort\r\n", ch);
	send_to_char("   nobard            noranger\r\n", ch);
	send_to_char("   bless\r\n\r\n", ch);
	send_to_char("Example :  oset glow\r\n", ch);
	send_to_char("           oset magic\r\n", ch);
	break;
  case 5:
	send_to_char("Usage   :  oset <wear flag>\r\n\r\n", ch);
	send_to_char("This will toggle the flag you specify.\r\n", ch);
	send_to_char("Valid wear flags are:\r\n", ch);
	send_to_char("   take		   arms\r\n", ch);
	send_to_char("   finger		   shield\r\n", ch);
	send_to_char("   neck		   about\r\n", ch);
	send_to_char("   body		   waist\r\n", ch);
	send_to_char("   head		   wrist\r\n", ch);
	send_to_char("   legs		   wield\r\n", ch);
	send_to_char("   feet		   hold\r\n", ch);
	send_to_char("   hands\r\n\r\n", ch);
	send_to_char("Example :  oset take\r\n", ch);
	send_to_char("           oset wield\r\n", ch);
	break;
  case 6:
    if(!*val_arg) {
      send_to_char("Usage   :  oset v0 <value>\r\n", ch);
      send_to_char("v0-v3 are based on object type.\r\n", ch);
      return;
    }
    OBJ_NEW(ch)->obj_flags.value[0] = atoi(val_arg);
    half_chop(val_arg, field, val_arg);
    break;
  case 7:
    if(!*val_arg) {
      send_to_char("Usage   :  oset v1 <value>\r\n", ch);
      send_to_char("v0-v3 are based on object type.\r\n", ch);
      return;
    }
    OBJ_NEW(ch)->obj_flags.value[1] = atoi(val_arg);
    half_chop(val_arg, field, val_arg);
    break;
  case 8:
    if(!*val_arg) {
      send_to_char("Usage   :  oset v2 <value>\r\n", ch);
      send_to_char("v0-v3 are based on object type.\r\n", ch);
      return;
    }
    OBJ_NEW(ch)->obj_flags.value[2] = atoi(val_arg);
    half_chop(val_arg, field, val_arg);
    break;
  case 9:
    if(!*val_arg) {
      send_to_char("Usage   :  oset v3 <value>\r\n", ch);
      send_to_char("v0-v3 are based on object type.\r\n", ch);
      return;
    }
    OBJ_NEW(ch)->obj_flags.value[3] = atoi(val_arg);
    half_chop(val_arg, field, val_arg);
    break;
  case 10:
    if(!*val_arg || !isdigit(*val_arg)) {
      send_to_char("Usage   :  oset weight <value>\r\n", ch);
      send_to_char("Use this to set an object's weight in pounds.\r\n", ch);
      return;
    }
    OBJ_NEW(ch)->obj_flags.weight = atoi(val_arg);
    half_chop(val_arg, field, val_arg);
    break;
  case 11:
    if(!*val_arg || !isdigit(*val_arg)) {
      send_to_char("Usage   :  oset cost <value>\r\n", ch);
      send_to_char("This sets an object's monetary value in gold coins.\r\n", ch);
      return;
    }
    OBJ_NEW(ch)->obj_flags.cost = atoi(val_arg);
    half_chop(val_arg, field, val_arg);
    break;
  case 12:
    if(!*val_arg || !isdigit(*val_arg)) {
      send_to_char("Usage   :  oset rent <value>\r\n", ch);
      send_to_char("This sets an objects cost per day when rented.\r\n", ch);
      return;
    }
    OBJ_NEW(ch)->obj_flags.cost_per_day = atoi(val_arg);
    half_chop(val_arg, field, val_arg);
    break;
  case 13:
	GET_OBJ_TYPE(OBJ_NEW(ch)) = ITEM_LIGHT;
	break;
  case 14:
	GET_OBJ_TYPE(OBJ_NEW(ch)) = ITEM_SCROLL;
	break;
  case 15:
	GET_OBJ_TYPE(OBJ_NEW(ch))= ITEM_WAND;
	break;
  case 16:
	GET_OBJ_TYPE(OBJ_NEW(ch)) = ITEM_STAFF;
	break;
  case 17:
	GET_OBJ_TYPE(OBJ_NEW(ch)) = ITEM_WEAPON;
	break;
  case 18:
	GET_OBJ_TYPE(OBJ_NEW(ch)) = ITEM_TREASURE;
	break;
      case 19:
	GET_OBJ_TYPE(OBJ_NEW(ch)) = ITEM_ARMOR;
	break;
      case 20:
	GET_OBJ_TYPE(OBJ_NEW(ch)) = ITEM_POTION;
	break;
      case 21:
	GET_OBJ_TYPE(OBJ_NEW(ch)) = ITEM_WORN;
	break;
      case 22:
	GET_OBJ_TYPE(OBJ_NEW(ch)) = ITEM_OTHER;
	break;
      case 23:
	GET_OBJ_TYPE(OBJ_NEW(ch)) = ITEM_TRASH;
	break;
      case 24:
	GET_OBJ_TYPE(OBJ_NEW(ch)) = ITEM_CONTAINER;
	break;
      case 25:
	GET_OBJ_TYPE(OBJ_NEW(ch)) = ITEM_NOTE;
	break;
      case 26:
	GET_OBJ_TYPE(OBJ_NEW(ch)) = ITEM_DRINKCON;
	break;
      case 27:
	GET_OBJ_TYPE(OBJ_NEW(ch)) = ITEM_KEY;
	break;
      case 28:
	GET_OBJ_TYPE(OBJ_NEW(ch)) = ITEM_FOOD;
	break;
      case 29:
	GET_OBJ_TYPE(OBJ_NEW(ch)) = ITEM_MONEY;
	break;
      case 30:
	GET_OBJ_TYPE(OBJ_NEW(ch)) = ITEM_PEN;
	break;
  case 31:
	GET_OBJ_TYPE(OBJ_NEW(ch)) = ITEM_BOAT;
	break;
  case 32:
	GET_OBJ_TYPE(OBJ_NEW(ch)) = ITEM_FOUNTAIN;
	break;
  case 33:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.extra_flags, ITEM_GLOW);
    break;
  case 34:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.extra_flags, ITEM_HUM);
    break;
  case 35:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.extra_flags, ITEM_NORENT);
    break;
  case 36:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.extra_flags, ITEM_NODONATE);
    break;
  case 37:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.extra_flags, ITEM_NOINVIS);
    break;
  case 38:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.extra_flags, ITEM_INVISIBLE);
    break;
  case 39:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.extra_flags, ITEM_MAGIC);
    break;
  case 40:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.extra_flags, ITEM_NODROP);
    break;
  case 41:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.extra_flags, ITEM_BLESS);
    break;
  case 42:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.extra_flags, ITEM_ANTI_GOOD);
    break;
  case 43:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.extra_flags, ITEM_ANTI_EVIL);
    break;
  case 44:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.extra_flags, ITEM_ANTI_NEUTRAL);
    break;
  case 45:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.extra_flags, ITEM_ANTI_MAGIC_USER);
    break;
  case 46:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.extra_flags, ITEM_ANTI_CLERIC);
    break;
  case 47:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.extra_flags, ITEM_ANTI_THIEF);
    break;
  case 48:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.extra_flags, ITEM_ANTI_KNIGHT);
    break;
  case 49:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.extra_flags, ITEM_NOSELL);
    break;
  case 50:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.wear_flags, ITEM_WEAR_TAKE);
    break;
  case 51:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.wear_flags, ITEM_WEAR_FINGER);
    break;
  case 52:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.wear_flags, ITEM_WEAR_NECK);
    break;
  case 53:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.wear_flags, ITEM_WEAR_BODY);
    break;
  case 54:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.wear_flags, ITEM_WEAR_HEAD);
    break;
  case 55:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.wear_flags, ITEM_WEAR_LEGS);
    break;
  case 56:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.wear_flags, ITEM_WEAR_FEET);
    break;
  case 57:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.wear_flags, ITEM_WEAR_HANDS);
    break;
  case 58:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.wear_flags, ITEM_WEAR_ARMS);
    break;
  case 59:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.wear_flags, ITEM_WEAR_SHIELD);
    break;
  case 60:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.wear_flags, ITEM_WEAR_ABOUT);
    break;
  case 61:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.wear_flags, ITEM_WEAR_WAIST);
    break;
  case 62:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.wear_flags, ITEM_WEAR_WRIST);
    break;
  case 63:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.wear_flags, ITEM_WEAR_WIELD);
    break;
  case 64:
    TOGGLE_BIT(OBJ_NEW(ch)->obj_flags.wear_flags, ITEM_WEAR_HOLD);
    break;
  case 65:
    if(!*val_arg) {
      send_to_char("Usage   :  oset action <object action description>\r\n", ch);
      send_to_char("An object's action description is the text you see when it is used.\r\n", ch);
      send_to_char("The circle documention on this is incomplete so no info on syntax\r\n", ch);
      send_to_char("is available.  If the object is a note, the action description\r\n", ch);
      send_to_char("is the text on the note.\r\n\r\n", ch);
      break;
    }
    OBJ_NEW(ch)->action_description = str_dup(val_arg);
    *val_arg = '\0';
    break;
  case 66:
  case 67:
  case 68:
  case 69:
  case 70:
  case 71:
  case 72:
  case 73:
  case 74:
    break;
  case 75:
    half_chop(val_arg, arg1, arg2);
    for (length = strlen(arg1), cmmd = 0; *affs[cmmd] != '\n'; cmmd++) 
      if (!strn_cmp(affs[cmmd], arg1, length))  
	break;
    if(*arg2 == '+') {
      neg_affect = FALSE;
      *arg2 = '0';
    } else if(*arg2 == '-') {
      neg_affect = TRUE;
      *arg2 = '0';
    } else neg_affect = FALSE;

    if(!*arg1 || (!*arg2 && (cmmd != 0)) ||((cmmd != 0)&&!isdigit(*arg2))) {
      send_to_char("Usage  : oset affect <affect> <value>\r\n", ch);
      send_to_char("Valid affect fields are:\r\n", ch);
      send_to_char("str      ac\r\n", ch);
      send_to_char("dex      hitroll\r\n", ch);
      send_to_char("int      damroll\r\n", ch);
      send_to_char("wis      paralysis (save throw)\r\n", ch);
      send_to_char("con      rod (save throw)\r\n", ch);
      send_to_char("cha      petrify (save throw)\r\n", ch);
      send_to_char("age      breath (save throw)\r\n", ch);
      send_to_char("weight   spell (save throw)\r\n", ch);
      send_to_char("height   none\r\n", ch);
      send_to_char("mana     \r\n", ch);
      send_to_char("hit      \r\n", ch);
      send_to_char("move     \r\n\r\n", ch);
      send_to_char("The field 'none' will remove all affects.  Any other affect with a\r\n", ch);
      send_to_char("value of 0 will rmove that affect.  The maximum number of affects\r\n", ch);
      send_to_char("for any object is 6.\r\n\r\n", ch);
      send_to_char("Example : oset affect str 2\r\n", ch);
      return;
    }
    affmod = atoi(arg2);
    switch(cmmd) {
      case 0:
	for(i = 0; i < MAX_OBJ_AFFECT; i++) {
	  OBJ_NEW(ch)->affected[i].location = 0;
	  OBJ_NEW(ch)->affected[i].modifier = 0;
	}
	afftype = 0;
	affmod = 0;
	half_chop(val_arg, arg1, val_arg);
	break;
      case 1:
	afftype = 1;
	break;
      case 2:
	afftype = 2;
	break;
      case 3:
	afftype = 3;
	break;
      case 4:
	afftype = 4;
	break;
      case 5:
	afftype = 5;
	break;
      case 6:
	afftype = 6;
	break;
      case 7:
	afftype = 9;
	break;
      case 8:
	afftype = 10;
	break;
      case 9:
	afftype = 11;
	break;
      case 10:
	afftype = 12;
	break;
      case 11:
	afftype = 13;
	break;
      case 12:
	afftype = 14;
	break;
      case 13:
	afftype = 17;
	break;
      case 14:
	afftype = 18;
	break;
      case 15:
	afftype = 19;
	break;
      case 16:
	afftype = 20;
	break;
      case 17:
	afftype = 21;
	break;
      case 18:
	afftype = 22;
	break;
      case 19:
	afftype = 23;
	break;
      case 20:
	afftype = 24;
	break;
      default:
	send_to_char("Invalid affect field.  type 'oset affect' for help\r\n", ch);
	afftype = 0;
	affmod = 0;
	help = TRUE;
	half_chop(arg2, arg1, val_arg);
    }
    if(afftype !=0) {
      if(affmod == 0) {
	for(i = 0; i < MAX_OBJ_AFFECT; i++) {
	  if(OBJ_NEW(ch)->affected[i].location == afftype) {
	    OBJ_NEW(ch)->affected[i].location = 0;
	    OBJ_NEW(ch)->affected[i].modifier = 0;
	  }
	}
      } else {
	for(i = 0; i < MAX_OBJ_AFFECT; i++) {
	  if((OBJ_NEW(ch)->affected[i].location == 0) ||
	      (OBJ_NEW(ch)->affected[i].location == afftype)) {
	    OBJ_NEW(ch)->affected[i].location = afftype;
	    if(neg_affect) affmod = 0 - affmod;
	    OBJ_NEW(ch)->affected[i].modifier = affmod;
	    break;
	  }
	}
	if(i == MAX_OBJ_AFFECT)
	  send_to_char("Affect not added.  All affect slots full.\r\n", ch);
      }
    half_chop(arg2, arg1, val_arg);
    }
    break;
  case 76:
    if(!*val_arg) {
      send_to_char("Usage  : oset edesc <command> <keyword list>\r\n\r\n", ch);
      send_to_char("To add an extra description, use oset edesc add <keyword list>, as in:\r\n", ch);
      send_to_char("oset edesc add inscription hilt\r\n\r\n", ch);
      send_to_char("To set the description text, use oset edesc set <keyword>, as in:\r\n", ch);
      send_to_char("oset edesc set hilt\r\n\r\n", ch);
      send_to_char("Use no doublequotes, and use only one keyword.\r\n\r\n", ch);
      send_to_char("To delete an extra description, use oset edesc del <keyword>, as in:\r\n", ch);
      send_to_char("oset del hilt\r\n\r\n", ch);
      send_to_char("To show the description text, use oset edesc show <keyword>, as in:\r\n", ch);
      send_to_char("oset edesc show hilt\r\n\r\n", ch);
      break;
    }
    half_chop(val_arg, arg1, arg2);

    if(is_abbrev(arg1, "add")) {
      if(!*arg2) {
	send_to_char("Keyword list missing.  Type oset edesc for help.\r\n", ch);
	return;
      }
      CREATE(desc, struct extra_descr_data, 1);
      desc->keyword = str_dup(arg2);
      desc->description = str_dup("You see nothing special.\r\n");
      desc->next = OBJ_NEW(ch)->ex_description;
      OBJ_NEW(ch)->ex_description = desc;
      *val_arg = '\0';
      break;
    }
    if(is_abbrev(arg1, "del")) {
      if(!*arg2) {
	send_to_char("Keyword missing.  Type oset edesc for help.\r\n", ch);
	return;
      }
      for(desc = OBJ_NEW(ch)->ex_description; desc; desc=desc->next) {
	if(isname(arg2, desc->keyword)) {
	  if(!last) {
	    if(OBJ_NEW(ch)->ex_description->next)
	      OBJ_NEW(ch)->ex_description = OBJ_NEW(ch)->ex_description->next;
	    else OBJ_NEW(ch)->ex_description = NULL;
	    free(desc);
	  } else {
	    if(desc->next) last->next = desc->next;
	    else last->next = NULL;
	    free(desc);
	  }
	  return;
	}
	last = desc;
      }
      send_to_char("No keyword match found.\r\n", ch);
      return;
    }
    if(is_abbrev(arg1, "set")) {
      if(!*arg2) {
	send_to_char("Keyword missing.  Type oset edesc for help.\r\n", ch);
	return;
      }
      for(desc = OBJ_NEW(ch)->ex_description; desc; desc=desc->next) {
	if(isname(arg2, desc->keyword)) {

#if CONFIG_IMPROVED_EDITOR == FALSE
	  send_to_char("Enter the extra description.  Use a @ at the beginning of a line\r\n", ch);
	  send_to_char("to save.\r\n\r\n", ch);
	  if(desc->description)
	    free(desc->description);
	  CREATE(desc->description, char, 2048);
#else
	  send_to_char("Enter the extra description.  /s to save, /h for help\r\n", ch);
	  if(desc->description) {
	    ch->desc->backstr = str_dup(desc->description);
	    send_to_char(desc->description, ch);
	  }
#endif
	  ch->desc->str = &(desc->description);
	  ch->desc->max_str = 2048;
	  SET_BIT(PLR_FLAGS(ch), PLR_WRITING);
	  return;
	}
      }
      send_to_char("No keyword match found.\r\n", ch);
      return;
    }
    if(is_abbrev(arg1, "show")) {
      if(!*arg2) {
	send_to_char("Keyword missing.  Type oset edesc for help.\r\n", ch);
	return;
      }
      for(desc = OBJ_NEW(ch)->ex_description; desc; desc=desc->next) {
	if(isname(arg2, desc->keyword)) {
	  if(!desc->description) {
	    send_to_char("No extra desc set for that keyword.\r\n", ch);
	    return;
	  }
	  send_to_char(desc->description, ch);
	  return;
	}
      }
      send_to_char("No keyword match found.\r\n", ch);
      return;
    }
    send_to_char("Invalid edesc argument.  Type oset edesc for help.\r\n", ch);
    return;
  case 77:
    break;
  default:
    if(!help) {
    send_to_char("Usage   :  oset <field> <value> [field2...]\r\n", ch);
    send_to_char("Valid fields are:\r\n", ch);
    send_to_char("name      object alias list (ex: oset name short sword)\r\n", ch);
    send_to_char("short     short description (ex: oset short a short sword)\r\n", ch);
    send_to_char("long      long description \r\n", ch);
    send_to_char("action    action description\r\n", ch);
    send_to_char("edesc     extra descriptions\r\n", ch);
    send_to_char("type      object type\r\n", ch);
    send_to_char("extra     extra effects bitvector\r\n", ch);
    send_to_char("wear      wear bitvector\r\n", ch);
    send_to_char("v0        value0 }\r\n", ch);
    send_to_char("v1        value1 }\r\n", ch);
    send_to_char("v2        value2 }-- object type dependent\r\n", ch);
    send_to_char("v3        value3 }\r\n", ch);
    send_to_char("weight    object weight\r\n", ch);
    send_to_char("cost      value of object in gold coins\r\n", ch);
    send_to_char("rent      rent per day in gold\r\n", ch);
    send_to_char("affect    affections\r\n", ch);
    send_to_char("oset will accept multiple fields, up to MAX_INPUT_LENGTH.\r\n", ch);
    send_to_char("The fields name, short, long, action, and edesc can't be followed by\r\n", ch);
    send_to_char("additional fields.\r\n", ch);
    send_to_char("For detailed field help, use: oset <field>\r\n", ch);
    send_to_char("Example : oset weapon glow magic weight 5 v0 2", ch);
    }
    help = TRUE;
  }
  half_chop(val_arg, field, val_arg);
  }  while(*field);
  if(!help) send_to_char(OK, ch);
}


ACMD(do_mset)
{
  int cmmd = 0, length = 0, help = FALSE;
  char field[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH];
  int neg_affect = FALSE;
  struct char_data *mob;
  int t[3];

  char *fields[] = 
    {   "alias"     ,"short"      ,"long"        ,"detail"     ,/*0-3*/
	"flags"     ,"affects"    ,"align"       ,"level"      ,
	"hitroll"   ,"damroll"    ,"ac"          ,"damage"   ,
	"hitpoints" ,"reserved"   ,"sentinel"    ,"scavenger"  ,/*12-15*/
	"aware"     ,"aggressive" ,"stayzone"    ,"wimpy"      ,
	"aggevil"   ,"agggood"    ,"aggneutral"  ,"memory"     ,
	"helper"    ,"nocharm"    ,"nosummon"    ,"nosleep"    ,/*24-27*/
	"nobash"    ,"noblind"    ,"reserved"    ,"reserved"   ,
	"reserved"  ,"blind"      ,"invisible"   ,"detalign"   ,
	"detinvis"  ,"detmagic"   ,"senselife"   ,"waterwalk"  ,/*36-39*/
	"sanctuary" ,"curse"      ,"infravis"    ,"poison"     ,
	"protfrevil","protfrgood" ,"sleep"       ,"notrack"    ,
	"reserved"  ,"reserved"   ,"sneak"       ,"hide"       ,/*48-51*/
	"reserved"  ,"reserved"   ,"reserved"    ,"reserved"   ,
	"gold"      ,"experience" ,"loadpos"     ,"defaultpos" ,
	"attacktype","sex"        ,"strength"    ,"stradd"     ,
	"intel"     ,"wisdom"     ,"dexterity"   ,"con"        ,
	"charisma"  ,
	"\n" };

  half_chop(argument, field, val_arg);

  if(!MOB_NEW(ch)) {
    send_to_char("There is no mob in the edit buffer.  Use medit to set one.\r\n\r\n", ch);
    *val_arg = '*';
    return;
  }

  mob = MOB_NEW(ch);

  if (!*field) *field = '*';

  do {
  for (length = strlen(field), cmmd = 0; *fields[cmmd] != '\n'; cmmd++) 
    if (!strn_cmp(fields[cmmd], field, length))  
      break;

  switch (cmmd) {
  case 0:
    if(!*val_arg) {
      send_to_char("Usage   :   mset alias <alias list>\r\n\r\n", ch);
      send_to_char("<alias list> should be a list of names players can use to act on the\r\n", ch);
      send_to_char("mob.\r\n\r\n", ch);
      send_to_char("Example :   mset name demon guard\r\n", ch);
      break;
    }
    mob->player.name = str_dup(val_arg);
    *val_arg = '\0';
    break;
  case 1:
    if(!*val_arg) {
      send_to_char("Usage   :  mset short <mob name>\r\n", ch);
      send_to_char("Example :  mset short The demon guard\r\n", ch);
      break;
    }
    mob->player.short_descr = str_dup(val_arg);
    *val_arg = '\0';
    break;
  case 2:
    if(!*val_arg) {
      send_to_char("Usage   :  mset long <mob long description>\r\n", ch);
      send_to_char("A mob's long description is the text you see when it is in it's default\r\n", ch);
      send_to_char("position.  The first word should be capitalized, and it should end\r\n", ch);
      send_to_char("with a period.\r\n\r\n", ch);
      send_to_char("Example :  mset long A demon duard stands here guarding the portal.\r\n", ch);
      break;
    }
    mob->player.long_descr = str_dup(val_arg);
    strcat(mob->player.long_descr, "\n");
    *val_arg = '\0';
    break;
  case 3:

#if CONFIG_IMPROVED_EDITOR == FALSE
    send_to_char("Enter new description.  Use a @ at the beginning of a line\r\n", ch);
    send_to_char("to save.\r\n\r\n", ch);
    if(mob->player.description)
      free(mob->player.description);
    CREATE(mob->player.description, char, 2048);
#else
    send_to_char("Enter new description.  /s to save, /h for help\r\n", ch);
    if(mob->player.description) {
      ch->desc->backstr = str_dup(mob->player.description);
      send_to_char(mob->player.description, ch);
    }
#endif
    ch->desc->str = &(mob->player.description);
    ch->desc->max_str = 2048;
    SET_BIT(PLR_FLAGS(ch), PLR_WRITING);
    return;
  case 4:
	send_to_char("Usage   :  mset <action flag>\r\n\r\n", ch);
	send_to_char("This will toggle the flag you specify.\r\n", ch);
	send_to_char("Valid action flags are:\r\n\r\n", ch);
	send_to_char("   sentinel          scavenger\r\n", ch);
	send_to_char("   aware             aggressive\r\n", ch);
	send_to_char("   stayzone          wimpy\r\n", ch);
	send_to_char("   aggevil           agggood\r\n", ch);
	send_to_char("   aggneutral        memory\r\n", ch);
	send_to_char("   helper            nocharm\r\n", ch);
	send_to_char("   nosummon          nosleep\r\n", ch);
	send_to_char("   nobash            noblind\r\n\r\n", ch);
	send_to_char("Example :  mset sentinel\r\n", ch);
	send_to_char("           mset stayzone nobash\r\n", ch);
	break;
  case 5:
	send_to_char("Usage   :  mset <affect flag>\r\n\r\n", ch);
	send_to_char("This will toggle the flag you specify.\r\n", ch);
	send_to_char("Valid affect flags are:\r\n\r\n", ch);
	send_to_char("   blind             invisible\r\n", ch);
	send_to_char("   detalign          detinvis\r\n", ch);
	send_to_char("   detmagic          senselife\r\n", ch);
	send_to_char("   waterwalk         sanctuary\r\n", ch);
	send_to_char("   curse             infravision\r\n", ch);
	send_to_char("   poison            protfrevil\r\n", ch);
	send_to_char("   protfrgood        sleep\r\n", ch);
	send_to_char("   notrack           \r\n", ch);
	send_to_char("   sneak             hide\r\n", ch);
	send_to_char("   \r\n\r\n", ch);
	send_to_char("Example :  mset invisible\r\n", ch);
	send_to_char("           mset senselife infravision\r\n", ch);
	break;
  case 6:
    if(!*val_arg || (!isdigit(*val_arg) && *val_arg != '-' &&
      *val_arg != '+')) {
      send_to_char("Usage   :  mset align <value>\r\n", ch);
      return;
    }
    if(*val_arg == '+') {
      neg_affect = FALSE;
      *val_arg = '0';
    } else if(*val_arg == '-') {
      neg_affect = TRUE;
      *val_arg = '0';
    } else neg_affect = FALSE;

    if(neg_affect)
      GET_ALIGNMENT(MOB_NEW(ch)) = 0 - atoi(val_arg);
    else
      GET_ALIGNMENT(MOB_NEW(ch)) = atoi(val_arg);
    half_chop(val_arg, field, val_arg);
    break;
  case 7:
    if(!*val_arg || !isdigit(*val_arg)) {
      send_to_char("Usage   :  mset level <mob level>\r\n", ch);
      return;
    }
    GET_LEVEL(MOB_NEW(ch)) = atoi(val_arg);
    if(GET_LEVEL(MOB_NEW(ch)) < 1)
      GET_LEVEL(MOB_NEW(ch)) = 1;
    if(GET_LEVEL(MOB_NEW(ch)) > 101)
      GET_LEVEL(MOB_NEW(ch)) = 101;
    half_chop(val_arg, field, val_arg);
    break;
  case 8:
    if(!*val_arg || (!isdigit(*val_arg) && *val_arg != '-' &&
      *val_arg != '+')) {
      send_to_char("Usage   :  mset hitroll <value>\r\n", ch);
      return;
    }
    if(*val_arg == '+') {
      neg_affect = FALSE;
      *val_arg = '0';
    } else if(*val_arg == '-') {
      neg_affect = TRUE;
      *val_arg = '0';
    } else neg_affect = FALSE;

    if(neg_affect)
      MOB_NEW(ch)->points.hitroll = 0 - atoi(val_arg);
    else
      MOB_NEW(ch)->points.hitroll = atoi(val_arg);
    half_chop(val_arg, field, val_arg);
    break;
  case 9:
    if(!*val_arg || (!isdigit(*val_arg) && *val_arg != '-' &&
      *val_arg != '+')) {
      send_to_char("Usage   :  mset damroll <value>\r\n", ch);
      return;
    }
    if(*val_arg == '+') {
      neg_affect = FALSE;
      *val_arg = '0';
    } else if(*val_arg == '-') {
      neg_affect = TRUE;
      *val_arg = '0';
    } else neg_affect = FALSE;

    if(neg_affect)
      MOB_NEW(ch)->points.damroll = 0 - atoi(val_arg);
    else
      MOB_NEW(ch)->points.damroll = atoi(val_arg);
    half_chop(val_arg, field, val_arg);
    break;
  case 10:
    if(!*val_arg || (!isdigit(*val_arg) && *val_arg != '-' &&
      *val_arg != '+')) {
      send_to_char("Usage   :  mset ac <value>\r\n", ch);
      return;
    }
    if(*val_arg == '+') {
      neg_affect = FALSE;
      *val_arg = '0';
    } else if(*val_arg == '-') {
      neg_affect = TRUE;
      *val_arg = '0';
    } else neg_affect = FALSE;

    if(neg_affect)
      MOB_NEW(ch)->points.armor = 0 - atoi(val_arg);
    else
      MOB_NEW(ch)->points.armor = atoi(val_arg);
    half_chop(val_arg, field, val_arg);
    break;
  case 11:
    if(!*val_arg || sscanf(val_arg, " %dd%d ", t, t + 1) !=2) {
      send_to_char("Usage   :  mset damage XdY\r\n\r\n", ch);
      send_to_char("Example :  mset damage 2d5\r\n", ch);
      return;
    }
    MOB_NEW(ch)->mob_specials.damnodice = t[0];
    MOB_NEW(ch)->mob_specials.damsizedice = t[1];
    half_chop(val_arg, field, val_arg);
    break;
  case 12:
    if(!*val_arg || sscanf(val_arg, " %dd%d+%d ", t, t + 1, t + 2) !=3) {
      send_to_char("Usage   :  mset hitpoints XdY+Z\r\n\r\n", ch);
      send_to_char("Example :  mset hitpoints 2d4+30\r\n", ch);
      return;
    }
    MOB_NEW(ch)->points.hit = t[0];
    MOB_NEW(ch)->points.mana = t[1];
    MOB_NEW(ch)->points.move = t[2];
    half_chop(val_arg, field, val_arg);
    break;
  case 13:
	break;
  case 14:
	TOGGLE_BIT(MOB_FLAGS(mob), MOB_SENTINEL);
	break;
  case 15:
	TOGGLE_BIT(MOB_FLAGS(mob), MOB_SCAVENGER);
	break;
  case 16:
	TOGGLE_BIT(MOB_FLAGS(mob), MOB_AWARE);
	break;
  case 17:
	TOGGLE_BIT(MOB_FLAGS(mob), MOB_AGGRESSIVE);
	break;
  case 18:
	TOGGLE_BIT(MOB_FLAGS(mob), MOB_STAY_ZONE);
	break;
  case 19:
	TOGGLE_BIT(MOB_FLAGS(mob), MOB_WIMPY);
	break;
  case 20:
	TOGGLE_BIT(MOB_FLAGS(mob), MOB_AGGR_EVIL);
	break;
  case 21:
	TOGGLE_BIT(MOB_FLAGS(mob), MOB_AGGR_GOOD);
	break;
  case 22:
	TOGGLE_BIT(MOB_FLAGS(mob), MOB_AGGR_NEUTRAL);
	break;
  case 23:
	TOGGLE_BIT(MOB_FLAGS(mob), MOB_MEMORY);
	break;
  case 24:
	TOGGLE_BIT(MOB_FLAGS(mob), MOB_HELPER);
	break;
  case 25:
	TOGGLE_BIT(MOB_FLAGS(mob), MOB_NOCHARM);
	break;
  case 26:
	TOGGLE_BIT(MOB_FLAGS(mob), MOB_NOSUMMON);
	break;
  case 27:
	TOGGLE_BIT(MOB_FLAGS(mob), MOB_NOSLEEP);
	break;
  case 28:
	TOGGLE_BIT(MOB_FLAGS(mob), MOB_NOBASH);
	break;
  case 29:
	TOGGLE_BIT(MOB_FLAGS(mob), MOB_NOBLIND);
	break;
  case 30:
  case 31:
  case 32:
	break;
  case 33:
    TOGGLE_BIT(AFF_FLAGS(mob), AFF_BLIND);
    break;
  case 34:
    TOGGLE_BIT(AFF_FLAGS(mob), AFF_INVISIBLE);
    break;
  case 35:
    TOGGLE_BIT(AFF_FLAGS(mob), AFF_DETECT_ALIGN);
    break;
  case 36:
    TOGGLE_BIT(AFF_FLAGS(mob), AFF_DETECT_INVIS);
    break;
  case 37:
    TOGGLE_BIT(AFF_FLAGS(mob), AFF_DETECT_MAGIC);
    break;
  case 38:
    TOGGLE_BIT(AFF_FLAGS(mob), AFF_SENSE_LIFE);
    break;
  case 39:
    TOGGLE_BIT(AFF_FLAGS(mob), AFF_WATERWALK);
    break;
  case 40:
    TOGGLE_BIT(AFF_FLAGS(mob), AFF_SANCTUARY);
    break;
  case 41:
    TOGGLE_BIT(AFF_FLAGS(mob), AFF_CURSE);
    break;
  case 42:
    TOGGLE_BIT(AFF_FLAGS(mob), AFF_INFRAVISION);
    break;
  case 43:
    TOGGLE_BIT(AFF_FLAGS(mob), AFF_POISON);
    break;
  case 44:
    TOGGLE_BIT(AFF_FLAGS(mob), AFF_PROTECT_EVIL);
    break;
  case 45:
    TOGGLE_BIT(AFF_FLAGS(mob), AFF_PROTECT_EVIL);
    break;
  case 46:
    TOGGLE_BIT(AFF_FLAGS(mob), AFF_SLEEP);
    break;
  case 47:
    TOGGLE_BIT(AFF_FLAGS(mob), AFF_NOTRACK);
    break;
  case 48:
  case 49:
    break;
  case 50:
    TOGGLE_BIT(AFF_FLAGS(mob), AFF_SNEAK);
    break;
  case 51:
    TOGGLE_BIT(AFF_FLAGS(mob), AFF_HIDE);
    break;
  case 52:
  case 53:
  case 54:
  case 55:
    break;
  case 56:
    if(!*val_arg || !isdigit(*val_arg)) {
      send_to_char("Usage   :  mset gold <number of coins>\r\n", ch);
      return;
    }
    GET_GOLD(MOB_NEW(ch)) = atoi(val_arg);
    half_chop(val_arg, field, val_arg);
    break;
  case 57:
    if(!*val_arg || !isdigit(*val_arg)) {
      send_to_char("Usage   :  mset experience <exp value of mob>\r\n", ch);
      return;
    }
    GET_EXP(MOB_NEW(ch)) = atoi(val_arg);
    half_chop(val_arg, field, val_arg);
    break;
  case 58:
    if(!*val_arg || !isdigit(*val_arg)) {
      send_to_char("Usage   :  mset loadpos <starting position od mob>\r\n\r\n", ch);
      send_to_char("Valid positions are:\r\n\r\n", ch);
      send_to_char("4  sleeping\r\n", ch);
      send_to_char("5  resting\r\n", ch);
      send_to_char("6  sitting\r\n", ch);
      send_to_char("8  standing\r\n", ch);
      return;
    }
    t[0] = atoi(val_arg);
    if((t[0] < 4 || t[0] > 6) && t[0] != 8)
      MOB_NEW(ch)->char_specials.position = 8;
    else
      MOB_NEW(ch)->char_specials.position = t[0];
    half_chop(val_arg, field, val_arg);
    break;
  case 59:
    if(!*val_arg || !isdigit(*val_arg)) {
      send_to_char("Usage   :  mset defaultpos <position mob takes after fighting>\r\n\r\n", ch);
      send_to_char("Valid positions are:\r\n\r\n", ch);
      send_to_char("4  sleeping\r\n", ch);
      send_to_char("5  resting\r\n", ch);
      send_to_char("6  sitting\r\n", ch);
      send_to_char("8  standing\r\n\r\n", ch);
      send_to_char("Note that when the mob is in its default position, looking at the room\r\n", ch);
      send_to_char("will show the mob's long description.\r\n", ch);
      return;
    }
    t[0] = atoi(val_arg);
    if((t[0] < 4 || t[0] > 6) && t[0] != 8)
      MOB_NEW(ch)->mob_specials.default_pos = 8;
    else
      MOB_NEW(ch)->mob_specials.default_pos = t[0];
    half_chop(val_arg, field, val_arg);
    break;
  case 60:
    if(!*val_arg || !isdigit(*val_arg)) {
      send_to_char("Usage   :  mset attacktype <mob's barehand attack type>\r\n\r\n", ch);
      send_to_char("Valid attack types are:\r\n\r\n", ch);
      send_to_char("0   hits\r\n", ch);
      send_to_char("1   stings\r\n", ch);
      send_to_char("2   whips\r\n", ch);
      send_to_char("3   slashes\r\n", ch);
      send_to_char("4   bites\r\n", ch);
      send_to_char("5   bludgeons\r\n", ch);
      send_to_char("6   crushes\r\n", ch);
      send_to_char("7   pounds\r\n", ch);
      send_to_char("8   claws\r\n", ch);
      send_to_char("9   mauls\r\n", ch);
      send_to_char("10  thrashes\r\n", ch);
      send_to_char("11  pierces\r\n", ch);
      send_to_char("12  blasts\r\n", ch);
      send_to_char("13  punches\r\n", ch);
      send_to_char("14  stabs\r\n\r\n", ch);
      send_to_char("Note that these attack types are used only when the mob is not\r\n", ch);
      send_to_char("wielding a weapon.\r\n", ch);
      return;
    }
    t[0] = atoi(val_arg);
    if(t[0] < 0 || t[0] > 14)
      MOB_NEW(ch)->mob_specials.attack_type = 0;
    else
      MOB_NEW(ch)->mob_specials.attack_type = t[0];
    half_chop(val_arg, field, val_arg);
    break;
  case 61:
    if(!*val_arg || !isdigit(*val_arg)) {
      send_to_char("Usage   :  mset sex <gender>\r\n\r\n", ch);
      send_to_char("Valid genders are:\r\n\r\n", ch);
      send_to_char("0  neutered (it)\r\n", ch);
      send_to_char("1  male\r\n", ch);
      send_to_char("2  female\r\n", ch);
      return;
    }
    t[0] = atoi(val_arg);
    if(t[0] < 0 || t[0] > 2)
      MOB_NEW(ch)->player.sex = 0;
    else
      MOB_NEW(ch)->player.sex = t[0];;
    half_chop(val_arg, field, val_arg);
    break;
  case 62:
    if(!*val_arg || !isdigit(*val_arg)) {
      send_to_char("Usage   :  mset strength <strength>\r\n", ch);
      return;
    }
    t[0] = atoi(val_arg);
    if(t[0] < 3 || t[0] > 25)
      MOB_NEW(ch)->real_abils.str = 11;
    else
      MOB_NEW(ch)->real_abils.str = t[0];
    half_chop(val_arg, field, val_arg);
    break;
  case 63:
    if(!*val_arg || !isdigit(*val_arg)) {
      send_to_char("Usage   :  mset stradd <strength addition>\r\n", ch);
      return;
    }
    t[0] = atoi(val_arg);
    if(t[0] < 0 || t[0] > 100)
      MOB_NEW(ch)->real_abils.str_add = 0;
    else
      MOB_NEW(ch)->real_abils.str_add = t[0];
    half_chop(val_arg, field, val_arg);
    break;
  case 64:
    if(!*val_arg || !isdigit(*val_arg)) {
      send_to_char("Usage   :  mset intel <intelligence>\r\n", ch);
      return;
    }
    t[0] = atoi(val_arg);
    if(t[0] < 3 || t[0] > 25)
      MOB_NEW(ch)->real_abils.intel = 11;
    else
      MOB_NEW(ch)->real_abils.intel = t[0];
    half_chop(val_arg, field, val_arg);
    break;
  case 65:
    if(!*val_arg || !isdigit(*val_arg)) {
      send_to_char("Usage   :  mset wisdom <wisdom>\r\n", ch);
      return;
    }
    t[0] = atoi(val_arg);
    if(t[0] < 3 || t[0] > 25)
      MOB_NEW(ch)->real_abils.wis = 11;
    else
      MOB_NEW(ch)->real_abils.wis = t[0];
    half_chop(val_arg, field, val_arg);
    break;
  case 66:
    if(!*val_arg || !isdigit(*val_arg)) {
      send_to_char("Usage   :  mset dexterity <dexterity>\r\n", ch);
      return;
    }
    t[0] = atoi(val_arg);
    if(t[0] < 3 || t[0] > 25)
      MOB_NEW(ch)->real_abils.dex = 11;
    else
      MOB_NEW(ch)->real_abils.dex = t[0];
    half_chop(val_arg, field, val_arg);
    break;
  case 67:
    if(!*val_arg || !isdigit(*val_arg)) {
      send_to_char("Usage   :  mset con <constitution>\r\n", ch);
      return;
    }
    t[0] = atoi(val_arg);
    if(t[0] < 3 || t[0] > 25)
      MOB_NEW(ch)->real_abils.con = 11;
    else
      MOB_NEW(ch)->real_abils.con = t[0];
    half_chop(val_arg, field, val_arg);
    break;
  case 68:
    if(!*val_arg || !isdigit(*val_arg)) {
      send_to_char("Usage   :  mset charisma <charisma>\r\n", ch);
      return;
    }
    t[0] = atoi(val_arg);
    if(t[0] < 3 || t[0] > 25)
      MOB_NEW(ch)->real_abils.cha = 11;
    else
      MOB_NEW(ch)->real_abils.cha = t[0];
    half_chop(val_arg, field, val_arg);
    break;
  default:
    if(!help) {
    send_to_char("Usage   :  mset <field> <value> [field2...]\r\n", ch);
    send_to_char("Valid fields are:\r\n", ch);
    send_to_char("alias     mob alias list (ex: mset alias guard demon)\r\n", ch);
    send_to_char("short     short description (ex: mset short The demon guard)\r\n", ch);
    send_to_char("long      long description (seen when you look room)\r\n", ch);
    send_to_char("detail    detailed description (seen when you look demon)\r\n", ch);
    send_to_char("flags       mob flags           ac        mob armor class\r\n", ch);
    send_to_char("affects     mob affects         hitpoints mob max hp\r\n", ch);
    send_to_char("align       mob alignment       damage    mob damage dice\r\n", ch);
    send_to_char("level       mob level           attacktype barehand attack type\r\n", ch);
    send_to_char("hitroll     mob hitroll         gold      mob pocket change\r\n", ch);
    send_to_char("damroll     mob damroll         experience mob exp point value\r\n", ch);
    send_to_char("loadpos     mob load position   sex       mob sex\r\n", ch);
    send_to_char("strength    mob strength        stradd    mob strength addition\r\n", ch);
    send_to_char("intel       mob intelligence    wisdom    mob wisdom\r\n", ch);
    send_to_char("dexterity   mob dexterity       con       mob constitution\r\n", ch);
    send_to_char("charisma    mob charisma        defaultpos  mob default pos\r\n\r\n", ch);
    send_to_char("mset will accept multiple fields, up to MAX_INPUT_LENGTH.\r\n\r\n", ch);
    send_to_char("The fields alias, short, long, and detail can't be followed by\r\n", ch);
    send_to_char("additional fields.\r\n", ch);
    send_to_char("For detailed field help, use: mset <field>\r\n", ch);
    send_to_char("Example : mset align -500 nocharm invisible\r\n", ch);
    }
    help = TRUE;
  }
  half_chop(val_arg, field, val_arg);
  }  while(*field);
  if(!help) send_to_char(OK, ch);
}


ACMD(do_rset)
{
  int cmmd = 0, length = 0, help = FALSE, top_room;
  char field[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH];
  char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
  char arg3[MAX_INPUT_LENGTH], arg4[MAX_INPUT_LENGTH];
  char arg5[MAX_INPUT_LENGTH];
  struct extra_descr_data *desc = NULL, *last = NULL;
  int direct, croom = NOWHERE, revdir = 0, i, r_num;

  char *fields[] = 
    {   "name"          ,"description"  ,"RESERVED"     ,
	"RESERVED"      ,"exit"         ,"edesc"        ,
	"RESERVED"      ,"flags"        ,"type"         ,
	"dark"          ,"death"        ,"nomob"        ,
	"indoors"       ,"peaceful"     ,"soundproof"   ,
	"notrack"       ,"nomagic"      ,"tunnel"       ,
	"private"       ,"godroom"      ,"reserved"     ,
	"reserved"      ,"reserved"     ,"reserved"     ,
	"inside"        ,"city"         ,"field"        ,
	"forest"        ,"hills"        ,"mountain"     ,
	"swim"          ,"noswim"       ,"underwater"   ,
	"flying"        ,"desert"       ,"coder"        ,
	"\n" };

char *efields[] =
  {     "key"           ,"description"  ,"nodoor"       ,
	"normal"        ,"pickproof"    ,"keyword"      ,
	"delete"        ,"connect"      ,"disconnect"   ,
	"reserved"      ,"reserved"     ,
	"\n" };

  if(ROOM_BUF(ch) == -1) {
    send_to_char("You must first enter the room editor with redit.\r\n", ch);
    return;
  }
  if(get_zon_num(world[ch->in_room].number) != ROOM_BUF(ch))  {
    send_to_char("This room is not in the zone you are editing.\r\n", ch);
    return;
  }
  
  half_chop(argument, field, val_arg);
  if(!*field) *field = '*';

do {
  for (length = strlen(field), cmmd = 0; *fields[cmmd] != '\n'; cmmd++) 
    if (!strn_cmp(fields[cmmd], field, length))  
      break;

  switch(cmmd) {
  case 0:
    if(!*val_arg) {
      send_to_char("Usage   : rset name <room name>\r\n\r\n", ch);
      send_to_char("Example : rset name The Bakery\r\n", ch);
      break;
    }
    world[ch->in_room].name = str_dup(val_arg);
    return;
  case 1:

#if CONFIG_IMPROVED_EDITOR == FALSE
    send_to_char("Enter new description.  Use a @ at the beginning of a line\r\n", ch);
    send_to_char("to save.\r\n\r\n", ch);
    if(world[ch->in_room].description)
      free(world[ch->in_room].description);
    CREATE(world[ch->in_room].description, char, 2048);
#else
    if(world[ch->in_room].description) {
      send_to_char("Enter new description.  /s to save, /h for help\r\n", ch);
      ch->desc->backstr = str_dup(world[ch->in_room].description);
      send_to_char(world[ch->in_room].description, ch);
    }
#endif
    ch->desc->str = &(world[ch->in_room].description);
    ch->desc->max_str = 2048;
    SET_BIT(PLR_FLAGS(ch), PLR_WRITING);
    return;
  case 4:
    half_chop(val_arg, arg1, arg4);
    half_chop(arg4, arg2, arg3);
    if(!*arg1 || !*arg2) {
      send_to_char("Usage   : rset exit <direction> <command> [arguments]\r\n\r\n", ch);
      send_to_char("Direction must be n, s, e, w, u, or d.  Valid commands are: \r\n\r\n", ch);
      send_to_char("keyword             valid names to use when opening/closing the door,\r\n", ch);
      send_to_char("                    if there is a door.\r\n", ch);
      send_to_char("description         the description seen when you look in that direction\r\n", ch);
      send_to_char("nodoor              no door here\r\n", ch);
      send_to_char("normal              a normal door that can be opened/closed/picked\r\n", ch);
      send_to_char("pickproof           door is pickproof\r\n", ch);
      send_to_char("key                 sets the vnumof the key for this door\r\n", ch);
      send_to_char("delete              remove this exit\r\n", ch);
      send_to_char("connect             connect this exit to a room\r\n", ch);
      send_to_char("disconnect          disconnect this exit from a room\r\n\r\n", ch);
      send_to_char("For deatiled help with connect/disconnect, type\r\n", ch);
      send_to_char("rset exit <dir> (dis)connect.\r\n\r\n", ch);
      send_to_char("Example : rset exit n keyword door\r\n", ch);
      send_to_char("	      rset exit n normal\r\n", ch);
      send_to_char("	      rset exit n connect 2 3001\r\n", ch);
      return;
    }
    /* for ne, nw, se, sw, etc change this switch statement to a series
	of if/else statements */
    switch(*arg1) {
      case 'n':
	direct = 0;
	break;
      case 'e':
	direct = 1;
	break;
      case 's':
	direct = 2;
	break;
      case 'w':
	direct = 3;
	break;
      case 'u':
	direct = 4;
	break;
      case 'd':
	direct = 5;
	break;
      default:
	send_to_char("Invalid direction. Type rset exit for help.\r\n", ch);
	return;
    }
    for (length = strlen(arg2), cmmd = 0; *efields[cmmd] != '\n'; cmmd++) 
      if (!strn_cmp(efields[cmmd], arg2, length))  
	break;
      switch(cmmd) {
	case 0:
	  if(!*arg3 || !isdigit(*arg3)) {
	    send_to_char("Usage: rset exit <dir> key <vnum>\r\n", ch);
	    return;
	  }
	  if(!world[ch->in_room].dir_option[direct])
	    if(!create_dir(ch->in_room, direct))
	      log("SYSERR: Invalid arguments passed to create_dir()");
	  world[ch->in_room].dir_option[direct]->key = atoi(arg3);
	  return;
	case 1:
	  if(!world[ch->in_room].dir_option[direct])
	    if(!create_dir(ch->in_room, direct))
	      log("SYSERR: Invalid arguments passed to create_dir()");

#if CONFIG_IMPROVED_EDITOR
	  send_to_char("Enter new description.  Use a @ at the beginning of a line\r\n", ch);
	  send_to_char("to save.\r\n\r\n", ch);
	  if(world[ch->in_room].dir_option[direct]->general_description)
	    free(world[ch->in_room].dir_option[direct]->general_description);
          CREATE(world[ch->in_room].dir_option[direct]->general_description, char, 2048);
#else
	  if(world[ch->in_room].dir_option[direct]->general_description) {
	    send_to_char("Enter new description.  /s to save, /h for help\r\n", ch);
	    ch->desc->backstr = str_dup(world[ch->in_room].dir_option[direct]->general_description);
	    send_to_char(world[ch->in_room].dir_option[direct]->general_description, ch);
	  }
#endif
	  ch->desc->str = &(world[ch->in_room].dir_option[direct]->general_description);
	  ch->desc->max_str = 1024;
	  SET_BIT(PLR_FLAGS(ch), PLR_WRITING);
	  return;
	case 2:
	  if(!world[ch->in_room].dir_option[direct])
	    if(!create_dir(ch->in_room, direct))
	      log("SYSERR: Invalid arguments passed to create_dir()");
	  world[ch->in_room].dir_option[direct]->exit_info = 0;
	  return;
	case 3:
	  if(!world[ch->in_room].dir_option[direct])
	    if(!create_dir(ch->in_room, direct))
	      log("SYSERR: Invalid arguments passed to create_dir()");
	  world[ch->in_room].dir_option[direct]->exit_info = EX_ISDOOR;
	  return;
	case 4:
	  if(!world[ch->in_room].dir_option[direct])
	    if(!create_dir(ch->in_room, direct))
	      log("SYSERR: Invalid arguments passed to create_dir()");
	  world[ch->in_room].dir_option[direct]->exit_info = EX_ISDOOR
	    | EX_PICKPROOF;
	  return;
	case 5:
	  if(!*arg3) {
	    if(world[ch->in_room].dir_option[direct] 
	      || !*world[ch->in_room].dir_option[direct]->keyword)
	      return;
	    free(world[ch->in_room].dir_option[direct]->keyword);
	    return;
	  }
	  if(!world[ch->in_room].dir_option[direct])
	    if(!create_dir(ch->in_room, direct))
	      log("SYSERR: Invalid arguments passed to create_dir()");
	  if(world[ch->in_room].dir_option[direct]->keyword != NULL)
	    free(world[ch->in_room].dir_option[direct]->keyword);
	  world[ch->in_room].dir_option[direct]->keyword = str_dup(arg3);
	  return;
	case 6:
	  if(!free_dir(ch->in_room, direct))
	    log("SYSERR: Invalid arguments to free_dir()");
	  return;
	case 7: /*connect*/
	  half_chop(arg3, arg4, arg5);
	  if(((*arg4 != '1') && (*arg4 != '2')) || (!(!*arg5) && 
	    !isdigit(*arg5))) {
	    send_to_char("Usage   : rset exit <dir> connect <1|2> [room]\r\n\r\n", ch);
	    send_to_char("The <1|2> argument should be 1 for a one-way connection, or 2 for a\r\n", ch);
	    send_to_char("two-way connection.  Two-way connections can't be made across zone\r\n", ch);
	    send_to_char("boundaries.\r\n\r\n[room] is the virtual number of the room you want to connect to.\r\n", ch);
	    send_to_char("If no room is specified, a new room will be created.\r\n\r\n", ch);
	    send_to_char("Example : rset exit n connect 1 3001\r\n", ch);
	    return;
	  }

	  if(*arg4 == '2')
	    revdir = rev_dir[direct];

	  if(!*arg5) {
	    if(new_rooms == max_new_rooms) {
	      send_to_char("No more new rooms allowed.  Reboot for more.\r\n", ch);
	      return;
	    }

	    top_room = get_top(ROOM_BUF(ch));

	    for(i = (ROOM_BUF(ch) * 100); i <= top_room; i++) {
	      if(i > top_room) {
		send_to_char("No free room vnums in this zone.\r\n", ch);
		return;
	      }
	      if(real_room(i) < 0) {
		top_of_world++;
		new_rooms++;
		croom = i;
		world[top_of_world].zone = ROOM_BUF(ch);
		world[top_of_world].number = croom;
		world[top_of_world].func = NULL;
		world[top_of_world].contents = NULL;
		world[top_of_world].people = NULL;
		world[top_of_world].light = 0;
		  world[top_of_world].ex_description = NULL;

		if(DEF_ROOM(ch) >= 0)
		  copy_room(top_of_world, DEF_ROOM(ch));
		else {
		  world[top_of_world].name = str_dup("A new room");
		  world[top_of_world].description = str_dup("You are in a room.\r\n");
		  world[top_of_world].room_flags = 0;
		  world[top_of_world].sector_type = 0;
		}

		for(i = 0; i < NUM_OF_DIRS; i++)
		  world[top_of_world].dir_option[i] = NULL;

		sprintf(buf, "Created new room #%i\r\n", croom);
		send_to_char(buf, ch);
		break;
	      }
	    }
	    if(!world[ch->in_room].dir_option[direct])
	      if(!create_dir(ch->in_room, direct))
	        log("SYSERR: Invalid arguments passed to create_dir()");
	    world[ch->in_room].dir_option[direct]->to_room =
		real_room(croom);
	    if(*arg4 == '2') {
	      if(!world[real_room(croom)].dir_option[revdir])
	        if(!create_dir(real_room(croom), revdir))
	          log("SYSERR: Invalid arguments passed to create_dir()");
	      world[real_room(croom)].dir_option[revdir]->to_room =
		ch->in_room;
	      world[real_room(croom)].dir_option[revdir]->exit_info = 
		world[ch->in_room].dir_option[direct]->exit_info;
	    }
	  } else {
	    croom = atoi(arg5);
	    if(croom < 0) {
	      send_to_char("A negative number?\r\n", ch);
	      return;
	    }
	    if((r_num = real_room(croom)) < 0) {
	      send_to_char("There is no room with that number.\r\n", ch);
	      return;
	    }
	    if(!world[ch->in_room].dir_option[direct])
	      if(!create_dir(ch->in_room, direct))
	        log("SYSERR: Invalid arguments passed to create_dir()");
	    world[ch->in_room].dir_option[direct]->to_room = r_num;
	    if(*arg4 == '2') {
	      if(get_zon_num(croom) != ROOM_BUF(ch)) {
		send_to_char("Second room is not in this zone.\r\n", ch);
		return;
	      }
	      if(!world[r_num].dir_option[revdir])
	        if(!create_dir(r_num, revdir))
	          log("SYSERR: Invalid arguments passed to create_dir()");
	      world[r_num].dir_option[revdir]->to_room = ch->in_room;
	      world[r_num].dir_option[revdir]->exit_info =
		world[ch->in_room].dir_option[direct]->exit_info;
	    }
	  }
	  return;
	case 8: /*disconnect*/
	  half_chop(arg3, arg4, arg5);
	  if((*arg4 != '1') && (*arg4 != '2')) {
	    send_to_char("Usage   : rset exit <dir> disconnect <1|2>\r\n\r\n", ch);
	    send_to_char("The <1|2> should be 1 to disconnect the exit room from this room.\r\n", ch);
	    send_to_char("Use a 2 to disconnect in both directions.\r\n", ch);
	    send_to_char("Double disconnects across zone boundaries are not permitted.\r\n\r\n", ch);
	    send_to_char("Example : rset exit n connect 1 3001\r\n", ch);
	    return;
	  }
	  if(*arg4 == '2')
	    revdir = rev_dir[direct];

	    if(world[ch->in_room].dir_option[direct] == NULL) {
	      send_to_char("That exit does not exist.\r\n", ch);
	      return;
	    }
	    r_num = world[ch->in_room].dir_option[direct]->to_room;
	    if(r_num < 0)
	      return;
	    world[ch->in_room].dir_option[direct]->to_room = NOWHERE;
	    if(*arg4 == '2') {
	      if(get_zon_num(world[r_num].number) != ROOM_BUF(ch)) {
		send_to_char("Can't disconnect outside of the zone.\r\n", ch);
		return;
	      }
	      if(world[r_num].dir_option[revdir])
		world[r_num].dir_option[revdir]->to_room = NOWHERE;
	    }
	  return;
	case 9:
	case 10:
	  return;
	default:
	  send_to_char("Invalid exit command.  Type rset exit for help.\r\n", ch);
	  return;
      }
  case 5:
    if(!*val_arg) {
      send_to_char("Usage   : rset edesc <command> <keyword list>\r\n\r\n", ch);
      send_to_char("To add an extra description, use rset edesc add <keyword list>, as in:\r\n", ch);
      send_to_char("rset edesc add sign\r\n\r\n", ch);
      send_to_char("To set the description text, use rset edesc set <keyword>, as in:\r\n", ch);
      send_to_char("rset edesc set sign\r\n\r\n", ch);
      send_to_char("Use no doublequotes, and use only one keyword.\r\n\r\n", ch);
      send_to_char("To delete an extra description, use rset edesc del <keyword>, as in:\r\n", ch);
      send_to_char("rset del sign\r\n\r\n", ch);
      send_to_char("To show the description text, use rset edesc show <keyword>, as in:\r\n", ch);
      send_to_char("rset edesc show sign\r\n\r\n", ch);
      break;
    }
    half_chop(val_arg, arg1, arg2);
    
    if(is_abbrev(arg1, "add")) {
      if(!*arg2) {
	send_to_char("Keyword list missing.  Type rset edesc for help.\r\n", ch);
	return;
      }
      CREATE(desc, struct extra_descr_data, 1);
      desc->keyword = str_dup(arg2);
      desc->description = str_dup("You see nothing special.\r\n");
      desc->next = world[ch->in_room].ex_description;
      world[ch->in_room].ex_description = desc;
      *val_arg = '\0';
      return;
    }
    if(is_abbrev(arg1, "del")) {
      if(!*arg2) {
	send_to_char("Keyword missing.  Type rset edesc for help.\r\n", ch);
	return;
      }
      for(desc = world[ch->in_room].ex_description; desc; desc=desc->next) {
	if(isname(arg2, desc->keyword)) {
	  if(!last) {
	    if(world[ch->in_room].ex_description->next)
	      world[ch->in_room].ex_description = world[ch->in_room].ex_description->next;
	    else world[ch->in_room].ex_description = NULL;
	    free(desc);
	  } else {
	    if(desc->next) last->next = desc->next;
	    else last->next = NULL;
	    free(desc);
	  }
	  return;
	}
	last = desc;
      }
      send_to_char("No keyword match found.\r\n", ch);
      return;
    }
    if(is_abbrev(arg1, "set")) {
      if(!*arg2) {
	send_to_char("Keyword missing.  Type rset edesc for help.\r\n", ch);
	return;
      }
      for(desc = world[ch->in_room].ex_description; desc; desc=desc->next) {
	if(isname(arg2, desc->keyword)) {

#if CONFIG_IMPROVED_EDITOR == FALSE
	  send_to_char("Enter new description.  Use a @ at the beginning of a line\r\n", ch);
	  send_to_char("to save.\r\n\r\n", ch);
	  if(desc->description)
	    free(desc->description);
	  CREATE(desc->description, char, 2048);
#else
	  if(desc->description) {
	    send_to_char("Enter new description.  /s to save, /h for help\r\n", ch);
	    ch->desc->backstr = str_dup(desc->description);
	    send_to_char(desc->description, ch);
	  }
#endif
	  ch->desc->str = &(desc->description);
	  ch->desc->max_str = 2048;
	  SET_BIT(PLR_FLAGS(ch), PLR_WRITING);
	  return;
	}
      }
      send_to_char("No keyword match found.\r\n", ch);
      return;
    }
    if(is_abbrev(arg1, "show")) {
      if(!*arg2) {
	send_to_char("Keyword missing.  Type rset edesc for help.\r\n", ch);
	return;
      }
      for(desc = world[ch->in_room].ex_description; desc; desc=desc->next) {
	if(isname(arg2, desc->keyword)) {
	  if(!desc->description) {
	    send_to_char("No extra desc set for that keyword.\r\n", ch);
	    return;
	  }
	  send_to_char(desc->description, ch);
	  return;
	}
      }
      send_to_char("No keyword match found.\r\n", ch);
      return;
    }
    send_to_char("Invalid edesc argument.  Type rset edesc for help.\r\n", ch);
    return;
  case 7:
    send_to_char("Usage   : rset <flag>\r\n\r\n", ch);
    send_to_char("Valid flags are:\r\n\r\n", ch);
    send_to_char("dark          death\r\n", ch);
    send_to_char("nomob         indoors\r\n", ch);
    send_to_char("peaceful      soundproof\r\n", ch);
    send_to_char("notrack       nomagic\r\n", ch);
    send_to_char("tunnel        private\r\n", ch);
    send_to_char("godroom       \r\n", ch);
    send_to_char("You may specify more than one flag on the command line.\r\n\r\n", ch);
    send_to_char("Example : rset dark tunnel\r\n", ch);
    break;
  case 8:
    send_to_char("Usage   : rset <room type>\r\n\r\n", ch);
    send_to_char("Valid room types are:\r\n\r\n", ch);
    send_to_char("inside        city\r\n", ch);
    send_to_char("field         forest\r\n", ch);
    send_to_char("hills         mountain\r\n", ch);
    send_to_char("swim          noswim\r\n", ch);
    send_to_char("underwater    flying\r\n", ch);
    send_to_char("\r\n\r\n", ch);
    send_to_char("Example : rset mountain\r\n", ch);
    break;
  case 9:
    TOGGLE_BIT(ROOM_FLAGS(ch->in_room), ROOM_DARK);
    break;
  case 10:
    TOGGLE_BIT(ROOM_FLAGS(ch->in_room), ROOM_DEATH);
    break;
  case 11:
    TOGGLE_BIT(ROOM_FLAGS(ch->in_room), ROOM_NOMOB);
    break;
  case 12:
    TOGGLE_BIT(ROOM_FLAGS(ch->in_room), ROOM_INDOORS);
    break;
  case 13:
    TOGGLE_BIT(ROOM_FLAGS(ch->in_room), ROOM_PEACEFUL);
    break;
  case 14:
    TOGGLE_BIT(ROOM_FLAGS(ch->in_room), ROOM_SOUNDPROOF);
    break;
  case 15:
    TOGGLE_BIT(ROOM_FLAGS(ch->in_room), ROOM_NOTRACK);
    break;
  case 16:
    TOGGLE_BIT(ROOM_FLAGS(ch->in_room), ROOM_NOMAGIC);
    break;
  case 17:
    TOGGLE_BIT(ROOM_FLAGS(ch->in_room), ROOM_TUNNEL);
    break;
  case 18:
    TOGGLE_BIT(ROOM_FLAGS(ch->in_room), ROOM_PRIVATE);
    break;
  case 19:
    TOGGLE_BIT(ROOM_FLAGS(ch->in_room), ROOM_GODROOM);
    break;
  case 20:
  case 21:
  case 22:
  case 23:
    break;
  case 24:
    GET_ROOM_SECTOR(ch->in_room) = SECT_INSIDE;
    break;
  case 25:
    GET_ROOM_SECTOR(ch->in_room) = SECT_CITY;
    break;
  case 26:
    GET_ROOM_SECTOR(ch->in_room) = SECT_FIELD;
    break;
  case 27:
    GET_ROOM_SECTOR(ch->in_room) = SECT_FOREST;
    break;
  case 28:
    GET_ROOM_SECTOR(ch->in_room) = SECT_HILLS;
    break;
  case 29:
    GET_ROOM_SECTOR(ch->in_room) = SECT_MOUNTAIN;
    break;
  case 30:
    GET_ROOM_SECTOR(ch->in_room) = SECT_WATER_SWIM;
    break;
  case 31:
    GET_ROOM_SECTOR(ch->in_room) = SECT_WATER_NOSWIM;
    break;
  case 32:
    GET_ROOM_SECTOR(ch->in_room) = SECT_UNDERWATER;
    break;
  case 33:
    GET_ROOM_SECTOR(ch->in_room) = SECT_FLYING;
    break;
  case 34:
  case 35:
    break;
  default:
    if(!help) {
    send_to_char("Usage   : rset <field|flag> [value]\r\n\r\n", ch);
    send_to_char("Valid fields/flags are:\r\n\r\n", ch);
    send_to_char("name          room name\r\n", ch);
    send_to_char("description   room description\r\n", ch);
    send_to_char("exit          add/change room exits\r\n", ch);
    send_to_char("edesc         extra descriptions\r\n", ch);
    send_to_char("flags         room flags\r\n", ch);
    send_to_char("type          room type\r\n\r\n", ch);
    send_to_char("rset will accept multiple flags/types on a single line.  Specifying\r\n", ch);
    send_to_char("a flag toggles that flag.  Setting a type overrides any previous type.\r\n\r\n", ch);
    send_to_char("For detailed field help help, type rset <field>\r\n\r\n", ch);
    send_to_char("Example : rset name The Temple Of Mencha\r\n", ch);
    send_to_char("          rset dark notrack inside\r\n", ch);
  }
  help = TRUE;
  }
  half_chop(val_arg, field, val_arg);
  } while(*field);
  if(!help) send_to_char(OK, ch);
}


ACMD(do_zset)
{
  int rzone, temp;
  char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];

  if(ZONE_BUF(ch) == -1) {
    send_to_char("You must first edit the zone editor with ZEDIT.\r\n", ch);
    return;
  }
  half_chop(argument, arg1, arg2);

  if(!*arg1) {
    send_to_char("Usage   : zset <option> [value]\r\n\r\n", ch);
    send_to_char("Valid options are:\r\n", ch);
    send_to_char("name          set zone name\r\n", ch);
    send_to_char("top           set top vnum\r\n", ch);
    send_to_char("lifespan      set number of ticks between resets\r\n", ch);
    send_to_char("type          set reset type\r\n\r\n", ch);
    send_to_char("For detailed help, type zset <option> with no value.\r\n", ch);
    return;
  }

  rzone = get_zone_rnum(ZONE_BUF(ch));

  if(is_abbrev(arg1, "name")) {
    if(!*arg2) {
      send_to_char("Usage   : zset name <zone name>\r\n", ch);
      send_to_char("Example : zset name Southwestern Midgaard\r\n", ch);
      return;
    }
    if(zone_table[rzone].name != NULL)
      free(zone_table[rzone].name);
    zone_table[rzone].name = str_dup(arg2);
    send_to_char(OK, ch);
    return;
  }
  if(is_abbrev(arg1, "top")) {
    if(GET_LEVEL(ch) < LVL_IMPL) {
      send_to_char("You're not holy enough to broaden your horizons\r\n", ch);
      return;
    }
    if(!*arg2 || !isdigit(*arg2)) {
      send_to_char("Usage   : zset top <top vnum>\r\n", ch);
      send_to_char("Example : zset top 3399\r\n", ch);
      return;
    }
    temp = atoi(arg2);

    if((temp < (zone_table[rzone].number * 100)) || (temp >=
      (zone_table[rzone + 1].number * 100))) {
      send_to_char("Invalid top number.\r\n", ch);
      return;
    }

    zone_table[rzone].top = temp;
    send_to_char(OK, ch);
    return;
  }
  if(is_abbrev(arg1, "lifespan")) {
    if(!*arg2 || !isdigit(*arg2)) {
      send_to_char("Usage   : zset lifespan <# of ticks>\r\n", ch);
      send_to_char("Example : zset lifespan 30\r\n", ch);
      return;
    }
    zone_table[rzone].lifespan = atoi(arg2);
    return;
  }
  if(is_abbrev(arg1, "type")) {
    if(!*arg2 || !isdigit(*arg2)) {
      send_to_char("Usage   : zset type <reset type>\r\n\r\n", ch);
      send_to_char("Valid reset types are:\r\n\r\n", ch);
      send_to_char("0       zone NEVER resets\r\n", ch);
      send_to_char("1       zone resets only when empty\r\n", ch);
      send_to_char("2       zone resets normally\r\n", ch);
      send_to_char("Example : zset type 2\r\n", ch);
      return;
    }
    temp = atoi(arg2);
    if(temp < 0 || temp > 2) temp = 2;
    zone_table[rzone].reset_mode = temp;
    send_to_char(OK, ch);
    return;
  }
}


ACMD(do_zdelete)
{
  int i, num, rzone;
  char arg[16];

  if(ZONE_BUF(ch) < 0) {
    send_to_char("You must first enter the zone editor with zedit.\r\n", ch);
    return;
  }

  one_argument(argument, arg);

  if(!*argument || !isdigit(*arg)) {
    send_to_char("Usage   : zdelete <command number>\r\n", ch);
    return;
  }

  num = atoi(arg);
  rzone = get_zone_rnum(ZONE_BUF(ch));

  for(i = 0; zone_table[rzone].cmd[i].command != 'S'; i++) {
    if(i == num) {
      delete_reset_com(rzone, num);
      send_to_char("Okay.", ch);
      return;
    }
  }
  send_to_char("That command number does not exist.\r\n", ch);
}


ACMD(do_zdoor)
{
  char arg1[MAX_INPUT_LENGTH];
  char arg2[MAX_INPUT_LENGTH];
  int i, rzone, direct, posit;
  struct reset_com *zcmd;

  char *dirs[] =
    {  "north", "east", "south", "west", "up", "down", "\n" };
  char *posits[] =
    {  "open", "closed", "locked", "\n" };

  if(ZONE_BUF(ch) < 0) {
    send_to_char("You must first enter the zone editor with zedit.\r\n", ch);
    return;
  }

  if(get_zon_num(world[ch->in_room].number) != ZONE_BUF(ch)) {
    send_to_char("This room is not in the zone you are editing.\r\n", ch);
    return;
  }

  rzone = get_zone_rnum(ZONE_BUF(ch));

  half_chop(argument, arg1, arg2);

  if(!*arg1 || !*arg2) {
    send_to_char("Usage   : zdoor <direction> <position>\r\n\r\n", ch);
    send_to_char("Direction must be a valid movement direction.\r\n\r\n", ch);
    send_to_char("Position must be one of the following:\r\n\r\n", ch);
    send_to_char("open      Door resets open\r\n", ch);
    send_to_char("closed    Door resets closed\r\n", ch);
    send_to_char("locked    Door resets closed and locked\r\n\r\n", ch);
    send_to_char("Example : zdoor n closed\r\n", ch);
    return;
  }

  for(direct = 0; !is_abbrev(arg1, dirs[direct]) && direct != 6; direct++);
  for(posit = 0; !is_abbrev(arg2, posits[posit]) && posit != 3; posit++);

  if(direct == 6 || posit == 3) {
    send_to_char("Invalid direction or door position.\r\n", ch);
    return;
  }

  if(!EXIT(ch, direct)) {
    send_to_char("There's no exit in that direction.\r\n", ch);
    return;
  }

  if(!(EXIT(ch, direct)->keyword) || EXIT(ch, direct)->exit_info == 0)
    send_to_char("Warning: That exit is not a door, or has no keyword.\r\n", ch);

  for(i = 0; zone_table[rzone].cmd[i].command != 'S'; i++) {
    if(zone_table[rzone].cmd[i].command == 'D' &&
       zone_table[rzone].cmd[i].arg1 == ch->in_room &&
       zone_table[rzone].cmd[i].arg2 == direct) {
      zone_table[rzone].cmd[i].arg3 = posit;
      send_to_char("Changed door position.\r\n", ch);
      return;
    }
  }

  CREATE(zcmd, struct reset_com, 1);
  zcmd->if_flag = FALSE;
  zcmd->command = 'D';
  zcmd->arg1 = ch->in_room;
  zcmd->arg2 = direct;
  zcmd->arg3 = posit;
  zcmd->line = 0;

  for(i = 0; zone_table[rzone].cmd[i].command != 'S' && 
    !(zone_table[rzone].cmd[i].command == 'M' &&
      zone_table[rzone].cmd[i].arg3 == ch->in_room) &&
    !(zone_table[rzone].cmd[i].command == 'O' &&
      zone_table[rzone].cmd[i].arg3 == ch->in_room) &&
    !(zone_table[rzone].cmd[i].command == 'D' &&
      zone_table[rzone].cmd[i].arg1 == ch->in_room); i++);
  add_reset_com(rzone, i, zcmd);
  send_to_char("Added door command.\r\n", ch);
}


ACMD(do_zequip)
{
  char arg1[MAX_INPUT_LENGTH];
  char arg2[MAX_INPUT_LENGTH];
  char arg3[MAX_INPUT_LENGTH];
  char arg4[MAX_INPUT_LENGTH];
  char arg5[MAX_INPUT_LENGTH];
  char arg6[MAX_INPUT_LENGTH];
  int i, rzone, mob_cmd, robject, posit;
  struct reset_com *zcmd;

  char *posits[] =
    { "light"     ,"1finger"    ,"2finger"    ,"1neck"     ,
      "2neck"     ,"body"       ,"head"       ,"legs"      ,
      "feet"      ,"hands"      ,"arms"       ,"shield"    ,
      "about"     ,"waist"      ,"1wrist"     ,"2wrist"    ,
      "wield"     ,"held"       ,"\n" };

  if(ZONE_BUF(ch) < 0) {
    send_to_char("You must first enter the zone editor with zedit.\r\n", ch);
    return;
  }

  if(get_zon_num(world[ch->in_room].number) != ZONE_BUF(ch)) {
    send_to_char("This room is not in the zone you are editing.\r\n", ch);
    return;
  }

  rzone = get_zone_rnum(ZONE_BUF(ch));

  half_chop(argument, arg1, arg2);
  half_chop(arg2, arg3, arg4);
  half_chop(arg4, arg5, arg6);

  if(!*arg1 || !*arg3 || !*arg5 || !isdigit(*arg1) || !isdigit(*arg3)) {
    send_to_char("Usage   : zequip <object vnum> <mob reset number> <position> [max existing]\r\n\r\n", ch);
    send_to_char("Mob reset number is the reset command number of the mob, as seen with\r\n", ch);
    send_to_char("the zflags command.  Position must be one of the following:\r\n\r\n", ch);
    send_to_char("light       1finger     2finger     1neck\r\n", ch);
    send_to_char("2neck       body        head        legs\r\n", ch);
    send_to_char("feet        hands       arms        shield\r\n", ch);
    send_to_char("about       waist       1wrist      2wrist\r\n", ch);
    send_to_char("wield       held\r\n\r\n", ch);
    send_to_char("Example : zequip 3021 5 wield 15\r\n\r\n", ch);
    send_to_char("Would equip the mob specified by reset command 5 with a short sword, which\r\n", ch);
    send_to_char("would be wielded, as long as there aren't already 15 short swords in\r\n", ch);
    send_to_char("the game.\r\n", ch);
    return;
  }

  if(!*arg6)
    sprintf(arg6, "10");

  mob_cmd = atoi(arg3);

  if((robject = real_object(atoi(arg1))) < 0) {
    send_to_char("That object does not exist.\r\n", ch);
    return;
  }

  for(posit = 0; !is_abbrev(arg5, posits[posit]) && posit != 18; posit++);

  if(posit == 18) {
    send_to_char("Invalid position.\r\n", ch);
    return;
  }

  for(i = 0; zone_table[rzone].cmd[i].command != 'S'; i++) {
    if(i == mob_cmd && zone_table[rzone].cmd[i].command == 'M') {
      CREATE(zcmd, struct reset_com, 1);
      zcmd->if_flag = TRUE;
      zcmd->command = 'E';
      zcmd->arg1 = robject;
      zcmd->arg2 = atoi(arg6);
      zcmd->arg3 = posit;
      zcmd->line = 0;
      add_reset_com(rzone, i + 1, zcmd);
      sprintf(arg4, "Equipping %s with %s on %s.\r\n", 
	mob_proto[zone_table[rzone].cmd[i].arg1].player.short_descr,
	obj_proto[robject].short_description, posits[posit]);
      send_to_char(arg4, ch);
      return;
    }
  }
  send_to_char("That mob doesn't exist.  Remember: use the reset command number, not the\r\n", ch);
  send_to_char("mob vnum.\r\n", ch);
}


ACMD(do_zgive)
{
  char arg1[MAX_INPUT_LENGTH];
  char arg2[MAX_INPUT_LENGTH];
  char arg3[MAX_INPUT_LENGTH];
  char arg4[MAX_INPUT_LENGTH];
  int i, rzone, mob_cmd, robject;
  struct reset_com *zcmd;

  if(ZONE_BUF(ch) < 0) {
    send_to_char("You must first enter the zone editor with zedit.\r\n", ch);
    return;
  }

  if(get_zon_num(world[ch->in_room].number) != ZONE_BUF(ch)) {
    send_to_char("This room is not in the zone you are editing.\r\n", ch);
    return;
  }

  rzone = get_zone_rnum(ZONE_BUF(ch));

  half_chop(argument, arg1, arg2);
  half_chop(arg2, arg3, arg4);

  if(!*arg1 || !*arg3 || !isdigit(*arg1) || !isdigit(*arg3)) {
    send_to_char("Usage   : zgive <object vnum> <mob reset number> [max existing]\r\n", ch);
    return;
  }

  if(!*arg4)
    sprintf(arg4, "10");

  mob_cmd = atoi(arg3);

  if((robject = real_object(atoi(arg1))) < 0) {
    send_to_char("That object does not exist.\r\n", ch);
    return;
  }

  for(i = 0; zone_table[rzone].cmd[i].command != 'S'; i++) {
    if(i == mob_cmd && zone_table[rzone].cmd[i].command == 'M') {
      CREATE(zcmd, struct reset_com, 1);
      zcmd->if_flag = TRUE;
      zcmd->command = 'G';
      zcmd->arg1 = robject;
      zcmd->arg2 = atoi(arg4);
      zcmd->line = 0;
      add_reset_com(rzone, i + 1, zcmd);
      sprintf(arg4, "Giving %s to %s.\r\n", 
	obj_proto[robject].short_description,
	mob_proto[zone_table[rzone].cmd[i].arg1].player.short_descr);
      send_to_char(arg4, ch);
      return;
    }
  }
  send_to_char("That mob doesn't exist.  Remember: use the reset command number, not the\r\n", ch);
  send_to_char("mob vnum.\r\n", ch);
}


ACMD(do_zload)
{
  char arg1[MAX_INPUT_LENGTH];
  char arg2[MAX_INPUT_LENGTH];
  char arg3[MAX_INPUT_LENGTH];
  char arg4[MAX_INPUT_LENGTH];
  int i, rzone, rmobject, temp, add_cmd = FALSE;
  struct reset_com *zcmd;

  zcmd = NULL;

  if(ZONE_BUF(ch) < 0) {
    send_to_char("You must first enter the zone editor with zedit.\r\n", ch);
    return;
  }

  if(get_zon_num(world[ch->in_room].number) != ZONE_BUF(ch)) {
    send_to_char("This room is not in the zone you are editing.\r\n", ch);
    return;
  }

  rzone = get_zone_rnum(ZONE_BUF(ch));

  half_chop(argument, arg1, arg2);
  half_chop(arg2, arg3, arg4);

  if(!*arg1 || !*arg3 || (!is_abbrev(arg1, "mob") && !is_abbrev(arg1, 
     "obj")) || !isdigit(*arg3)) {
    send_to_char("Usage   : zload <obj|mob> <obj/mob vnum> [max existing]\r\n", ch);
    return;
  }

  temp = atoi(arg3);
  if(!*arg4)
    sprintf(arg4, "10");

  if(is_abbrev(arg1, "mob")) {
    if((rmobject = real_mobile(temp)) < 0) {
      send_to_char("That mob does not exist.\r\n", ch);
      return;
    }

    CREATE(zcmd, struct reset_com, 1);
    zcmd->if_flag = FALSE;
    zcmd->command = 'M';
    zcmd->arg1 = rmobject;
    zcmd->arg2 = atoi(arg4);
    zcmd->arg3 = ch->in_room;
    zcmd->line = 0;
    add_cmd = TRUE;
  }

  if(is_abbrev(arg1, "obj")) {
    if((rmobject = real_object(temp)) < 0) {
      send_to_char("That object does not exist.\r\n", ch);
      return;
    }

    CREATE(zcmd, struct reset_com, 1);
    zcmd->if_flag = FALSE;
    zcmd->command = 'O';
    zcmd->arg1 = rmobject;
    zcmd->arg2 = atoi(arg4);
    zcmd->arg3 = ch->in_room;
    zcmd->line = 0;
    add_cmd = TRUE;
  }
  if(add_cmd) {
    for(i = 0; zone_table[rzone].cmd[i].command != 'S' && 
      !(zone_table[rzone].cmd[i].command == 'M' &&
	zone_table[rzone].cmd[i].arg3 == ch->in_room) &&
      !(zone_table[rzone].cmd[i].command == 'O' &&
	zone_table[rzone].cmd[i].arg3 == ch->in_room) &&
    !(zone_table[rzone].cmd[i].command == 'D' &&
      zone_table[rzone].cmd[i].arg1 == ch->in_room); i++);
    add_reset_com(rzone, i, zcmd);
    sprintf(arg4, "Loading %s #%d in this room.\r\n", is_abbrev(arg1, 
      "mob") ? "mob" : "object", atoi(arg3));
    send_to_char(arg4, ch);
  }
}

ACMD(do_zput)
{
  char arg1[MAX_INPUT_LENGTH];
  char arg2[MAX_INPUT_LENGTH];
  char arg3[MAX_INPUT_LENGTH];
  char arg4[MAX_INPUT_LENGTH];
  int i, rzone, obj_cmd, robject;
  struct reset_com *zcmd;
  char tcommand;

  if(ZONE_BUF(ch) < 0) {
    send_to_char("You must first enter the zone editor with zedit.\r\n", ch);
    return;
  }

  if(get_zon_num(world[ch->in_room].number) != ZONE_BUF(ch)) {
    send_to_char("This room is not in the zone you are editing.\r\n", ch);
    return;
  }

  rzone = get_zone_rnum(ZONE_BUF(ch));

  half_chop(argument, arg1, arg2);
  half_chop(arg2, arg3, arg4);

  if(!*arg1 || !*arg3 || !isdigit(*arg1) || !isdigit(*arg3)) {
    send_to_char("Usage   : zput <object vnum> <container reset num> [max existing]\r\n\r\n", ch);
    send_to_char("Container reset number is the reset command number of the container object,\r\n", ch);
    send_to_char("as seen with the zflags command.\r\n\r\n", ch);
    send_to_char("Example : zput 3021 5 15\r\n\r\n", ch);
    send_to_char("Would put a short sword in the object specified by reset command\r\n", ch);
    send_to_char("5, as long as there aren't already 15 short swords in the game\r\n", ch);
    return;
  }

  if(!*arg4)
    sprintf(arg4, "10");

  obj_cmd = atoi(arg3);

  if((robject = real_object(atoi(arg1))) < 0) {
    send_to_char("That object does not exist.\r\n", ch);
    return;
  }

  for(i = 0; zone_table[rzone].cmd[i].command != 'S'; i++) {
    tcommand = zone_table[rzone].cmd[i].command;
    if(i == obj_cmd && (tcommand == 'O' || tcommand == 'G' ||
       tcommand == 'E' || tcommand == 'P')) {
      CREATE(zcmd, struct reset_com, 1);
      zcmd->if_flag = TRUE;
      zcmd->command = 'P';
      zcmd->arg1 = robject;
      zcmd->arg2 = atoi(arg4);
      zcmd->arg3 = zone_table[rzone].cmd[i].arg1;
      zcmd->line = 0;
      add_reset_com(rzone, i + 1, zcmd);
      sprintf(arg4, "Putting %s in %s.\r\n", 
	obj_proto[robject].short_description,
	obj_proto[zcmd->arg3].short_description);
      send_to_char(arg4, ch);
      return;
    }
  }
  send_to_char("That container doesn't exist.  Remember: use the reset command number,\r\n", ch);
  send_to_char("not the object vnum.\r\n", ch);
}


ACMD(do_zremove)
{
  send_to_char("Sorry, this feature hasn't been conjured yet.\r\n", ch);
}


ACMD(do_copy)
{
  int rnum, vnum;
  char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];

  half_chop(argument, arg1, arg2);

  if(!*arg1 || !*arg2) {
    send_to_char("Usage: copy mob|obj|room [vnum]\r\n", ch);
    return;
  }

  if((vnum = atoi(arg2)) < 0) {
    send_to_char("Negative vnums are not allowed.\r\n", ch);
    return;
  }

  if(is_abbrev(arg1, "mob")) {
    if(!MOB_NEW(ch)) {
      send_to_char("You're not editing a mob to copy to.\r\n", ch);
      return;
    }
    if((rnum = real_mobile(vnum)) < 0) {
      send_to_char("Can't find that mob.\r\n", ch);
      return;
    }
    copy_mobile(MOB_NEW(ch), mob_proto + rnum);
    send_to_char(OK, ch);
    return;
  }

  if(is_abbrev(arg1, "obj")) {
    if(!OBJ_NEW(ch)) {
      send_to_char("You're not editing an object to copy to.\r\n", ch);
      return;
    }
    if((rnum = real_object(vnum)) < 0) {
      send_to_char("Can't find that object.\r\n", ch);
      return;
    }
    copy_object(OBJ_NEW(ch), obj_proto + rnum);
    send_to_char(OK, ch);
    return;
  }

  if(is_abbrev(arg1, "room")) {
    if(ROOM_BUF(ch) < 0) {
      send_to_char("You're not editing rooms.\r\n", ch);
      return;
    }
    if(get_zon_num(world[ch->in_room].number) != ROOM_BUF(ch)) {
      send_to_char("The room you're in isn't in the zone you're editing.\r\n", ch);
      return;
    }
    if((rnum = real_room(vnum)) < 0) {
      send_to_char("Can't find that room.\r\n", ch);
      return;
    }
    if(rnum == ch->in_room) {
      send_to_char("That would be pointless.\r\n", ch);
      return;
    }
    copy_room(ch->in_room, rnum);
    send_to_char(OK, ch);
    return;
  }
  send_to_char("Usage: copy mob|obj|room [vnum]\r\n", ch);
}


ACMD(do_zlist){
  int i, found = FALSE, rnum, zone, zn;
  char buf[MAX_STRING_LENGTH], arg[MAX_INPUT_LENGTH];

  one_argument(argument, arg);

  if(!*arg) {
    send_to_char("Usage: zlist mobs|objects|rooms\r\n", ch);
    return;
  }

  zone = get_zon_num(world[ch->in_room].number);
  zn = get_zone_rnum(zone);

  if(is_abbrev(arg, "mobs")) {
    sprintf(buf, "Mobs in zone #%d:\r\n\r\n", zone);
    for(i = zone * 100; i <= zone_table[zn].top; i++) {
      if((rnum = real_mobile(i)) >=0) {
	sprintf(buf, "%s%5d - %s\r\n", buf, i,
	  mob_proto[rnum].player.short_descr ?
	  mob_proto[rnum].player.short_descr : "Unnamed");
	found = TRUE;
      }
    }
  } else if(is_abbrev(arg, "objects")) {
    sprintf(buf, "Objects in zone #%d:\r\n\r\n", zone);
    for(i = zone * 100; i <= zone_table[zn].top; i++) {
      if((rnum = real_object(i)) >=0) {
	sprintf(buf, "%s%5d - %s\r\n", buf, i,
	  obj_proto[rnum].short_description ?
	  obj_proto[rnum].short_description : "Unnamed");
	found = TRUE;
      }
    }
  } else if(is_abbrev(arg, "rooms")) {
    sprintf(buf, "Rooms in zone #%d:\r\n\r\n", zone);
    for(i = zone * 100; i <= zone_table[zn].top; i++) {
      if((rnum = real_room(i)) >=0) {
	sprintf(buf, "%s%5d - %s\r\n", buf, i,
	  world[rnum].name ?
	  world[rnum].name : "Unnamed");
	found = TRUE;
      }
    }
  } else {
    send_to_char("Invalid argument.\r\n", ch);
    send_to_char("Usage: zlist mobs|objects|rooms\r\n", ch);
    return;
  }

  if(!found) sprintf(buf, "%sNone!\r\n", buf);

  page_string(ch->desc, buf, 1);
}


int get_line2(FILE * fl, char *buf)
{
  char temp[256];
  int lines = 0;

  do {
    lines++;
    fgets(temp, 256, fl);
    if (*temp)
      temp[strlen(temp) - 1] = '\0';
  } while (!feof(fl) && (!*temp));	/* Same as stock, but '*' isn't comment. */

  if (feof(fl))
    return 0;
  else {
    strcpy(buf, temp);
    return lines;
  }
}

int add_to_index(int zone, char * type)
{
  char *old_fname, *new_fname, line[256];
  FILE *old_file, *new_file;
  char *prefix, entry[16];
  int fdsign = FALSE, wrote = FALSE;
  int last = -1, iline;

  switch(*type) {
    case 'z':
      prefix = ZON_PREFIX;
      break;
    case 'w':
      prefix = WLD_PREFIX;
      break;
    case 'o':
      prefix = OBJ_PREFIX;
      break;
    case 'm':
      prefix = MOB_PREFIX;
      break;
    case 's':
      prefix = SHP_PREFIX;
      break;
    default:
      return(FALSE);
  }
  sprintf(entry, "%i.%s", zone, type);

  sprintf(buf1, "%s/index", prefix);
  old_fname = str_dup(buf1);
  sprintf(buf2, "%s/index.temp", prefix);
  new_fname = str_dup(buf2);

  if(!(old_file = fopen(old_fname, "r"))) {
    return(FALSE);
  }
  if(!(new_file = fopen(new_fname, "w"))) {
    fclose(old_file);
    return(FALSE);
  }

  do {
    if(!get_line2(old_file, line) && !wrote) {
      fclose(old_file);
      fclose(new_file);
      return(FALSE);
    }
    if(*line == '$') {
      fdsign = TRUE;
      if (!wrote) {
	if(fprintf(new_file, "%s\n$\n", entry) < 0) {
	  break;
	}
      wrote = TRUE;
      }
      fprintf(new_file, "$\n");
      break;
    }
    iline = atoi(line);
    if((last < zone) && (zone < iline)) {
      fprintf(new_file, "%s\n%s\n", entry, line);
      wrote = TRUE;
    } else fprintf(new_file, "%s\n", line);
    last = iline;
  } while(!fdsign && !feof(old_file));
  fclose(old_file);
  fclose(new_file);
  remove(old_fname);
  rename(new_fname, old_fname);
  return(TRUE);
}

int get_zone_perms(struct char_data * ch, int z_num)
{
  int allowed = FALSE, rnum;
  struct builder_list *bldr;

  if(GET_LEVEL(ch) < LVL_IMPL) {

/* check builder memory */
    if((rnum = get_zone_rnum(z_num)) == -1) {
      send_to_char("Error: Could not get zone real number.\r\n", ch);
      return -1;
    }

    if(zone_table[rnum].builders == NULL)
      read_zone_perms(rnum);

    /* check memory */
    for(bldr = zone_table[rnum].builders; bldr; bldr = bldr->next)
      if(!str_cmp(ch->player.name, bldr->name))
	allowed = TRUE;

    if(allowed == FALSE) {
      sprintf(buf, "You don't have builder rights in zone #%i\r\n",
	z_num);
      send_to_char(buf, ch);
      return(-1);
    }
  }
  return(z_num);  
}

int save_rooms(int zone)
{
  char fname[64], rflags[40];
  FILE *savefile;
  char backname[64];
  int i, j, r_num, v_num;
  int doorcode, doorstuff;
  struct extra_descr_data *desc;
  int top_room;
  char *temp = 0;

  sprintf(backname, "%s/%i.wld", WLD_PREFIX, zone);
  sprintf(fname, "%s/%i.wld.back", WLD_PREFIX, zone);

  if(!(savefile = fopen(fname, "w")))
    return(1);

  top_room = (zone * 100) + 99;

  for(i = 0; i <= top_of_zone_table && zone_table[i].number != zone; i++);
    if(i <= top_of_zone_table)
      top_room = zone_table[i].top;

  for(i = (zone * 100); i <= top_room; i++) {
    if((r_num = real_room(i) >= 0)) {

      r_num = real_room(i);

      *rflags = '\0';
      if(world[r_num].room_flags == 0) {
	sprintf(rflags, "0");
      } else
	  sprintbits(world[r_num].room_flags, rflags);
      fprintf(savefile, "#%i\n", i);
      fprintf(savefile, "%s~\n", CHECK_NULL(world[r_num].name));

      temp = str_dup(CHECK_NULL(world[r_num].description));
      kill_ems(temp);
      fprintf(savefile, "%s~\n", temp);
      free(temp);

      fprintf(savefile, "%i %s %i\n", zone, (*rflags != '\0') ? rflags :
	"0", world[r_num].sector_type);

      for(j = 0; j < NUM_OF_DIRS; j++) {
	if(world[r_num].dir_option[j]) {
	  if(world[r_num].dir_option[j]->to_room < 0)
	    v_num = -1;
	  else v_num = world[world[r_num].dir_option[j]->to_room].number;

	  temp = str_dup(CHECK_NULL(world[r_num].dir_option[j]->general_description));
	  kill_ems(temp); 
	  fprintf(savefile, "D%i\n%s~\n", j, temp);
	  free(temp);

	  fprintf(savefile, "%s~\n",
	    CHECK_NULL(world[r_num].dir_option[j]->keyword));
	  doorstuff = world[r_num].dir_option[j]->exit_info; 
	  if (IS_SET(doorstuff, EX_ISDOOR) && IS_SET(doorstuff, EX_PICKPROOF))
	   doorcode = 2;
	  else if (IS_SET(doorstuff, EX_ISDOOR))
	   doorcode = 1;
	  else doorcode = 0;
	  fprintf(savefile, "%i %i %i\n",
	    doorcode,
	    world[r_num].dir_option[j]->key,
	    v_num);
	}
      }

      if(world[r_num].ex_description) {
	for(desc = world[r_num].ex_description; desc; desc=desc->next) {
	  fprintf(savefile, "E\n%s~\n", CHECK_NULL(desc->keyword));

	  temp = str_dup(CHECK_NULL(desc->description));
	  kill_ems(temp);
	  fprintf(savefile, "%s~\n", temp);
	  free(temp);

	}
      }

      fprintf(savefile, "S\n");
    }
  }
  fprintf(savefile, "$\n");
  fclose(savefile);
  remove(backname);
  rename(fname, backname);
  return(0);
}


void save_zone(struct char_data * ch, int zone)
{
  char fname[64], backname[64], *descrip;
  FILE *zfile;
  int i, rzone, sdesc, arg1 = 0, arg2 = 0, arg3 = 0;
  struct builder_list *bldr;

  descrip = NULL;

  rzone = get_zone_rnum(zone);

  if(zone_table[rzone].builders == NULL)
    read_zone_perms(rzone);

  sdesc = comment_zone_file;

  sprintf(fname, "%s/%i.zon.temp", ZON_PREFIX, zone);
  sprintf(backname, "%s/%i.zon", ZON_PREFIX, zone);

  if(!(zfile = fopen(fname, "w"))) {
    send_to_char("Error:  Can't open zone file for write.\r\n", ch);
    return;
  }

  fprintf(zfile, "#%d\n", zone_table[rzone].number);
  fprintf(zfile, "%s~\n", CHECK_NULL(zone_table[rzone].name));
  fprintf(zfile, "%d %d %d\n", zone_table[rzone].top,
    zone_table[rzone].lifespan, zone_table[rzone].reset_mode);

  for(bldr = zone_table[rzone].builders; bldr; bldr = bldr->next)
    if(bldr->name && *(bldr->name) != '*')
      fprintf(zfile, "* Builder %s\n", bldr->name ? bldr->name :
	"#nobody");

  for(i = 0; zone_table[rzone].cmd[i].command != 'S'; i++) {
    switch (zone_table[rzone].cmd[i].command) {
      case 'M':
	descrip = mob_proto[zone_table[rzone].cmd[i].arg1].player.short_descr;
	arg1 = mob_index[zone_table[rzone].cmd[i].arg1].vnum;
	arg2 = zone_table[rzone].cmd[i].arg2;
	arg3 = world[zone_table[rzone].cmd[i].arg3].number;
	break;
      case 'O':
	descrip = obj_proto[zone_table[rzone].cmd[i].arg1].short_description;
	arg1 = obj_index[zone_table[rzone].cmd[i].arg1].vnum;
	arg2 = zone_table[rzone].cmd[i].arg2;
	arg3 = world[zone_table[rzone].cmd[i].arg3].number;
	break;
      case 'G':
	descrip = obj_proto[zone_table[rzone].cmd[i].arg1].short_description;
 	arg1 = obj_index[zone_table[rzone].cmd[i].arg1].vnum;
	arg2 = zone_table[rzone].cmd[i].arg2;
	arg3 = 0;
	break;
      case 'E':
	descrip = obj_proto[zone_table[rzone].cmd[i].arg1].short_description;
	arg1 = obj_index[zone_table[rzone].cmd[i].arg1].vnum;
	arg2 = zone_table[rzone].cmd[i].arg2;
	arg3 = zone_table[rzone].cmd[i].arg3;
	break;
      case 'P':
	descrip = obj_proto[zone_table[rzone].cmd[i].arg1].short_description;
	arg1 = obj_index[zone_table[rzone].cmd[i].arg1].vnum;
	arg2 = zone_table[rzone].cmd[i].arg2;
	arg3 = obj_index[zone_table[rzone].cmd[i].arg3].vnum;
	break;
      case 'D':
	descrip = NULL;
	arg1 = world[zone_table[rzone].cmd[i].arg1].number;
	arg2 = zone_table[rzone].cmd[i].arg2;
	arg3 = zone_table[rzone].cmd[i].arg3;
	break;
      case 'R':
	descrip = NULL;
	arg1 = world[zone_table[rzone].cmd[i].arg1].number;
	arg2 = obj_index[zone_table[rzone].cmd[i].arg2].vnum;
	arg3 = 0;
	break;
    }
    fprintf(zfile, "%c %d %5d %5d %5d%s%s\n",
	zone_table[rzone].cmd[i].command,
	zone_table[rzone].cmd[i].if_flag, arg1, arg2, arg3, (sdesc
	  && descrip) ? "          " : "", (sdesc && descrip) ? descrip : "");
    descrip = NULL;
  }
  fprintf(zfile, "S\n$\n");
  fclose(zfile);
  remove(backname);
  rename(fname, backname);
  send_to_char("Zone file saved.\r\n", ch);
}


/*check to see if an object is already being edited.*/
int check_edit_obj(struct char_data * ch, int obj_vnum)
{
  struct descriptor_data *d;
  struct char_data *tch;

  for(d = descriptor_list; d; d = d->next) {
    if(d->connected)
      continue;

    if(d->original)
      tch = d->original;
    else if(!(tch = d->character))
      continue;

    if(!OBJ_NEW(tch))
      continue;

    if(GET_OBJ_VNUM(OBJ_NEW(tch)) == obj_vnum) {
      sprintf(buf, "That object is being edited by %s\r\n", GET_NAME(tch));
      send_to_char(buf, ch);
      return(FALSE);
    }
  }
  return(TRUE);
}


/*check to see if a mob is already being edited.*/
int check_edit_mob(struct char_data * ch, int mob_vnum)
{
  struct descriptor_data *d;
  struct char_data *tch;

  for(d = descriptor_list; d; d = d->next) {
    if(d->connected)
      continue;

    if(d->original)
      tch = d->original;
    else if(!(tch = d->character))
      continue;

    if(!MOB_NEW(tch))
      continue;

    if(GET_MOB_VNUM(MOB_NEW(tch)) == mob_vnum) {
      sprintf(buf, "That mob is being edited by %s\r\n", GET_NAME(tch));
      send_to_char(buf, ch);
      return(FALSE);
    }
  }
  return(TRUE);
}


int check_edit_zone(struct char_data * ch, int zone_vnum)
{
  struct descriptor_data *d;
  struct char_data *tch;

  for(d = descriptor_list; d; d = d->next) {
    if(d->connected)
      continue;

    if(d->original)
      tch = d->original;
    else if(!(tch = d->character))
      continue;

    if(ZONE_BUF(tch) == zone_vnum) {
      sprintf(buf, "This zone is being edited by %s\r\n", GET_NAME(tch));
      send_to_char(buf, ch);
      return(FALSE);
    }
  }
  return(TRUE);
}


int check_edit_zrooms(struct char_data * ch, int zone_vnum)
{
  struct descriptor_data *d;
  struct char_data *tch;

  for(d = descriptor_list; d; d = d->next) {
    if(d->connected)
      continue;

    if(d->original)
      tch = d->original;
    else if(!(tch = d->character))
      continue;

    if(ROOM_BUF(tch) == zone_vnum) {
      sprintf(buf, "This zone is being edited by %s\r\n", GET_NAME(tch));
      send_to_char(buf, ch);
      return(FALSE);
    }
  }
  return(TRUE);
}


/* add zone to zone_table */
struct zone_data *add_zone(struct zone_data * zn_table, int zone)
{
  struct zone_data *tdata;
  int i, wrote = FALSE, last = 0, lz = -1;
  char tstr[128];

  tdata = zn_table;

  top_of_zone_table++;

  CREATE(zn_table, struct zone_data, top_of_zone_table + 1);

  zn_table[top_of_zone_table].number = 32000;

  for(i = 0; i <= top_of_zone_table; i++) {
    if(!wrote && zone > lz && zone < tdata[last].number) {
      sprintf(tstr, "Zone %d", zone);
      zn_table[i].name = str_dup(tstr);
      CREATE(zn_table[i].builders, struct builder_list, 1);
      zn_table[i].builders->name = str_dup("*");
      zn_table[i].builders->next = NULL;
      zn_table[i].number = zone;
      zn_table[i].top = (zone * 100) + 99;
      zn_table[i].lifespan = 25;
      zn_table[i].age = 0;
      zn_table[i].reset_mode = 2;
      CREATE(zn_table[i].cmd, struct reset_com, 1);
      zn_table[i].cmd[0].command = 'S';
      wrote = TRUE;
    } else {
      zn_table[i].name = tdata[last].name;
      zn_table[i].builders = tdata[last].builders;
      zn_table[i].number = tdata[last].number;
      zn_table[i].age = tdata[last].age;
      zn_table[i].lifespan = tdata[last].lifespan;
      zn_table[i].top = tdata[last].top;
      zn_table[i].reset_mode = tdata[last].reset_mode;
      zn_table[i].cmd = tdata[last].cmd;
      lz = tdata[last].number;
      last++;
    }
  }
  free(tdata);
  return zn_table;
}

/* check for the top room/obj/mob in a zone */
int get_top(int zone)
{
  int i, top;

  top = (zone * 100) + 99;

  for(i = 0; i <= top_of_zone_table && zone_table[i].number != zone; i++);
    if(i <= top_of_zone_table)
      top = zone_table[i].top;

  return top;
}


/* check for true zone number */
int get_zon_num(int vnum)
{
  int i, zn = -1, zone;

  zone = vnum / 100;

  for(i = 0; i <= top_of_zone_table && zone_table[i].number <= zone; i++);
    if(i <= top_of_zone_table || zone_table[i - 1].top >= vnum)
      zn = zone_table[i - 1].number;

  return zn;
}


/* return zone rnum */
int get_zone_rnum(int zone)
{
  int i;

  for(i = 0; i <= top_of_zone_table && zone_table[i].number != zone; i++);
    if(i > top_of_zone_table)
      return -1;
    else
      return i;
}


/* read builders from zone file into memory */
int read_zone_perms(int rnum)
{
  FILE *old_file;
  char *old_fname, line[256];
  char arg3[40], arg4[40], arg5[40], arg6[40];
  struct builder_list *bldr, *tbldr;
  int found = FALSE;

    bldr = zone_table[rnum].builders;

    sprintf(buf1, "%s/%i.zon", ZON_PREFIX, zone_table[rnum].number);
    old_fname = buf1;

    if(!(old_file = fopen(old_fname, "r"))) {
      sprintf(buf, "SYSERR: Could not open %s for read.\r\n", old_fname);
      log(buf);
      return(FALSE);
    }
    do {
      get_line2(old_file, line);
      if(*line == '*') {
	half_chop(line, arg3, arg4);
	half_chop(arg4, arg5, arg6);
	if(is_abbrev(arg5, "Builder")) {
	  found = TRUE;
	  if(!bldr) {
	    CREATE(bldr, struct builder_list, 1);
	    bldr->name = str_dup(arg6);
	    bldr->next = NULL;
	  } else {
	    CREATE(tbldr, struct builder_list, 1);
	    tbldr->name = str_dup(arg6);
	    tbldr->next = bldr;
	    bldr = tbldr;
	  }
	}
      }
    } while(*line != '$' && !feof(old_file));
    fclose(old_file);

    if(!bldr) {
      CREATE(bldr, struct builder_list, 1);
      bldr->name = str_dup("*");
      bldr->next = NULL;
    }
    zone_table[rnum].builders = bldr;

  return(TRUE);
}


/* show the zone resets in character's room */
void show_z_resets(struct char_data * ch, int min, int max)
{
  int i = 0, room, rznum = -1;
  int rroom = NOWHERE, zroom = NOWHERE;
  char buf[16384], tmp1[32], tmp2[32], tmp3[64];
  int list = FALSE;
  char *eq_types_short[] = {
    "light",
    "finger 1",
    "finger 2",
    "neck 1",
    "neck 2",
    "body",
    "head",
    "legs",
    "feet",
    "hands",
    "arms",
    "shield",
    "about",
    "waist",
    "wrist 1",
    "wrist 2",
    "wielded",
    "held",
    "\n"
  };

  room = world[ch->in_room].number;
  rznum = get_zone_rnum(get_zon_num(room));
  if((rroom = real_room(room)) == -1) {
    send_to_char("Error:  This room does not exist.\r\n", ch);
    return;
  }

  if(zone_table[rznum].cmd == NULL || zone_table[rznum].cmd[0].command == 'S') {
    send_to_char("Error:  No reset commands in this zone.\r\n", ch);
    return;
  }

  *buf = '\0';

  do {
    list = ((min > -1) && (i >= min));
    switch(zone_table[rznum].cmd[i].command) {
      case 'M':
	*tmp3 = '\0';
	zroom = zone_table[rznum].cmd[i].arg3;
	if((ZON_FLAGGED(ch, ZFL_MOBS) && zroom == rroom) || list) {
	  sprintf(buf, "%s%s%c Cmd %3d - Mob %5d - Max %3d - %s\r\n", buf, 
	    CCYEL(ch, C_SPR), zone_table[rznum].cmd[i].if_flag ? '*'
	     : ' ', i, mob_index[zone_table[rznum].cmd[i].arg1].vnum,
	    zone_table[rznum].cmd[i].arg2,
	    mob_proto[zone_table[rznum].cmd[i].arg1].player.short_descr);
	}
	break;
      case 'O':
	*tmp3 = '\0';
	zroom = zone_table[rznum].cmd[i].arg3;
	if((ZON_FLAGGED(ch, ZFL_OLOADS) && zroom == rroom) || list) {
	  sprintf(buf, "%s%s%c Cmd %3d - Obj %5d - Max %3d - %s\r\n", buf, 
	    CCGRN(ch, C_SPR), zone_table[rznum].cmd[i].if_flag ? '*'
	     : ' ', i, obj_index[zone_table[rznum].cmd[i].arg1].vnum,
	    zone_table[rznum].cmd[i].arg2, 
	    obj_proto[zone_table[rznum].cmd[i].arg1].short_description);
	}
	break;
      case 'P':
	sprintf(tmp3, "%s  ", tmp3);
	if((ZON_FLAGGED(ch, ZFL_OPUTS) && zroom == rroom) || list) {
	  sprintf(buf, "%s%s%c %sCmd %3d - Put Obj %5d in Obj %5d - Max %3d - %s\r\n"
	    , buf, CCBLU(ch, C_SPR), zone_table[rznum].cmd[i].if_flag ? '*'
	     : ' ', tmp3, i, 
	    obj_index[zone_table[rznum].cmd[i].arg1].vnum,
	    obj_index[zone_table[rznum].cmd[i].arg3].vnum,
	    zone_table[rznum].cmd[i].arg2, 
	    obj_proto[zone_table[rznum].cmd[i].arg1].short_description);
	}
	break;
      case 'R':
	*tmp3 = '\0';
	zroom = zone_table[rznum].cmd[i].arg1;
	if((ZON_FLAGGED(ch, ZFL_REMOVES) && zroom == rroom) || list) {
	  sprintf(buf, "%s%s%c Cmd %3d - Remove Obj %5d - %s\r\n", buf, 
	    CCRED(ch, C_SPR), zone_table[rznum].cmd[i].if_flag ? '*'
	     : ' ', i, obj_index[zone_table[rznum].cmd[i].arg2].vnum,
	    obj_proto[zone_table[rznum].cmd[i].arg2].short_description);
	}
	break;
      case 'E':
	sprintf(tmp3, "  ");
	if((ZON_FLAGGED(ch, ZFL_OEQUIPS) && zroom == rroom) || list) {
	  *tmp1 = '\0';
	  sprinttype(zone_table[rznum].cmd[i].arg3, eq_types_short, tmp1);

	  sprintf(buf, "%s%s%c   Cmd %3d - Equip Obj %5d - Max %3d - On %8s - %s\r\n"
	    , buf, CCCYN(ch, C_SPR), zone_table[rznum].cmd[i].if_flag ? '*'
	    : ' ', i, obj_index[zone_table[rznum].cmd[i].arg1].vnum,
	    zone_table[rznum].cmd[i].arg2, tmp1,
	    obj_proto[zone_table[rznum].cmd[i].arg1].short_description);
	}
	break;
      case 'G':
	sprintf(tmp3, "  ");
	if((ZON_FLAGGED(ch, ZFL_OGIVES) && zroom == rroom) || list) {
	  sprintf(buf, "%s%s%c   Cmd %3d - Give Obj %5d - Max %3d - %s\r\n", buf, 
	    CCWHT(ch, C_SPR), zone_table[rznum].cmd[i].if_flag ? '*'
	     : ' ', i, obj_index[zone_table[rznum].cmd[i].arg1].vnum,
	    zone_table[rznum].cmd[i].arg2, 
	    obj_proto[zone_table[rznum].cmd[i].arg1].short_description);
	}
	break;
      case 'D':
	*tmp3 = '\0';
	zroom = zone_table[rznum].cmd[i].arg1;
	if((ZON_FLAGGED(ch, ZFL_DOORS) && zroom == rroom) || list) {
	  *tmp1 = '\0';
	  *tmp2 = '\0';
	  sprinttype(zone_table[rznum].cmd[i].arg2, dirs, tmp1);
 	  sprinttype(zone_table[rznum].cmd[i].arg3, exit_bits, tmp2);

	  sprintf(buf, "%s%s%c Cmd %3d - Door %5s - %s\r\n", buf, 
	    CCMAG(ch, C_SPR), zone_table[rznum].cmd[i].if_flag ? '*'
	     : ' ', i, tmp1, tmp2);
	}
	break;
    }
    i++;
  } while((zone_table[rznum].cmd[i].command != 'S') && (!list || (i <=
    max || max == -1)));
  sprintf(buf, "%s%s", buf, CCNRM(ch, C_SPR));
  page_string(ch->desc, buf, 1);
}


/* remove a reset command */
void delete_reset_com(int zone, int com_num)
{
  struct reset_com *tcom;
  int i, last, commands;

  for(commands = 0;  zone_table[zone].cmd[commands].command != 'S'; 
    commands++);
  CREATE(tcom, struct reset_com, commands);

  for(i = 0, last = 0; i <= commands; i++) {
    if(i != com_num) {
      tcom[last] = zone_table[zone].cmd[i];
      last++;
    }
  }
  free(zone_table[zone].cmd);
  zone_table[zone].cmd = tcom;
}


/* add a reset command */
void add_reset_com(int zone, int com_num, struct reset_com * cmd)
{
  struct reset_com *tcom;
  int i, last, commands;

  for(commands = 0;  zone_table[zone].cmd[commands].command != 'S'; 
    commands++);
  commands += 2;
  CREATE(tcom, struct reset_com, commands);

  for(i = 0, last = 0; i < commands; i++) {
    if(i != com_num) {
      tcom[i] = zone_table[zone].cmd[last];
      last++;
    } else {
      tcom[i].if_flag = cmd->if_flag;
      tcom[i].command = cmd->command;
      tcom[i].arg1 = cmd->arg1;
      tcom[i].arg2 = cmd->arg2;
      tcom[i].arg3 = cmd->arg3;
      tcom[i].line = cmd->line;
    }
  }
  free(cmd);
  free(zone_table[zone].cmd);
  zone_table[zone].cmd = tcom;
}


/*thanks to Luis Carvalho for sending this my way..it's probably a
  lot shorter than the one I would have made :)  */
void sprintbits(long vektor,char *outstring)
{
  int i;
  char flags[53]="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

  strcpy(outstring,"");
  for (i=0;i<32;i++)
  {
    if (vektor & 1) {
      *outstring=flags[i];
      outstring++;
    };
    vektor>>=1;
  };
  *outstring=0;
};


int mobs_to_file(int zone)
{
  int i, rzone, type_e = FALSE, temproll, vmob;
  FILE *new_file;
  char new_fname[64];
  char backname[64];
  char actions[40], affects[40];
  char *temp = 0;

  rzone = get_zone_rnum(zone);

  sprintf(backname, "%s/%i.mob", MOB_PREFIX, zone);
  sprintf(new_fname, "%s/%i.mob.back", MOB_PREFIX, zone);

  if(!(new_file = fopen(new_fname, "w")))
    return 1;

  for(vmob = zone * 100; vmob <= zone_table[rzone].top; vmob++) {
    
    if((i = real_mobile(vmob)) >= 0) {
      if(mob_index[i].deleted == FALSE) {
	if((mob_proto[i].mob_specials.attack_type != 0) ||
	  (mob_proto[i].real_abils.str != 11) ||
	  (mob_proto[i].real_abils.str_add != 0) ||
	  (mob_proto[i].real_abils.dex != 11) ||
	  (mob_proto[i].real_abils.intel != 11) ||
	  (mob_proto[i].real_abils.wis != 11) ||
	  (mob_proto[i].real_abils.con != 11) ||
	  (mob_proto[i].real_abils.cha != 11))
	    type_e = TRUE;


	if(fprintf(new_file, "#%i\n", vmob) < 0) {
	  fclose(new_file);
	  return 1;
	}
	fprintf(new_file, "%s~\n", CHECK_NULL(mob_proto[i].player.name));
	fprintf(new_file, "%s~\n", CHECK_NULL(mob_proto[i].player.short_descr));
	fprintf(new_file, "%s~\n", CHECK_NULL(mob_proto[i].player.long_descr));

	temp = str_dup(CHECK_NULL(mob_proto[i].player.description));
	kill_ems(temp);
	fprintf(new_file, "%s~\n", temp);
	free(temp);

	*actions = '\0';
	*affects = '\0';
	if(MOB_FLAGS(mob_proto + i) == 0) {
	  sprintf(actions, "0");
	} else
	    sprintbits(MOB_FLAGS(mob_proto + i), actions);
	if(AFF_FLAGS(mob_proto + i) == 0) {
	  sprintf(affects, "0");
	} else
	    sprintbits(AFF_FLAGS(mob_proto + i), affects);
	if(mob_proto[i].points.hitroll < 0)
	  temproll = 20 - mob_proto[i].points.hitroll;
	else if(mob_proto[i].points.hitroll > 20)
	  temproll = 0 - (mob_proto[i].points.hitroll - 20);
	else temproll = 20 - mob_proto[i].points.hitroll;
	fprintf(new_file, "%s %s %i %c\n", actions, affects, 
	  GET_ALIGNMENT(mob_proto + i), type_e ? 'E' : 'S');
	fprintf(new_file, "%i %i %i %id%i+%i %id%i+%i\n", 
	  GET_LEVEL(mob_proto + i),
	  temproll, 
	  mob_proto[i].points.armor / 10, 
	  mob_proto[i].points.hit,
	  mob_proto[i].points.mana,
	  mob_proto[i].points.move,
	  mob_proto[i].mob_specials.damnodice,
	  mob_proto[i].mob_specials.damsizedice,
	  mob_proto[i].points.damroll);
	fprintf(new_file, "%d %d\n", GET_GOLD(mob_proto + i),
	  GET_EXP(mob_proto + i));
	fprintf(new_file, "%i %i %i\n", mob_proto[i].char_specials.position,
	  mob_proto[i].mob_specials.default_pos, mob_proto[i].player.sex);
	if(type_e) {
	  if(mob_proto[i].mob_specials.attack_type != 0)
	    fprintf(new_file, "BareHandAttack: %d\n", 
	      mob_proto[i].mob_specials.attack_type);
	  if(mob_proto[i].real_abils.str != 11)
	    fprintf(new_file, "Str: %d\n", mob_proto[i].real_abils.str);
	  if(mob_proto[i].real_abils.str_add != 0)
	    fprintf(new_file, "StrAdd: %d\n", mob_proto[i].real_abils.str_add);
	  if(mob_proto[i].real_abils.intel != 11)
	    fprintf(new_file, "Int: %d\n", mob_proto[i].real_abils.intel);
	  if(mob_proto[i].real_abils.wis != 11)
	    fprintf(new_file, "Wis: %d\n", mob_proto[i].real_abils.wis);
	  if(mob_proto[i].real_abils.dex != 11)
	    fprintf(new_file, "Dex: %d\n", mob_proto[i].real_abils.dex);
	  if(mob_proto[i].real_abils.con != 11)
	    fprintf(new_file, "Con: %d\n", mob_proto[i].real_abils.con);
	  if(mob_proto[i].real_abils.cha != 11)
	    fprintf(new_file, "Cha: %d\n", mob_proto[i].real_abils.cha);

	  fprintf(new_file, "E\n");
	  type_e = FALSE;
	}
      }
    }
  }
  fprintf(new_file, "$\n");
  fclose(new_file);
  remove(backname);
  rename(new_fname, backname);
  return 0;
}



int objs_to_file(int zone)
{
  int i, j, rzone, vobj;
  FILE *new_file;
  char new_fname[64], backname[64], extras[40], wears[40];
  struct extra_descr_data *desc = NULL;
  char *temp = NULL, *temp2 = NULL;

  rzone = get_zone_rnum(zone);

  sprintf(backname, "%s/%i.obj", OBJ_PREFIX, zone);
  sprintf(new_fname, "%s/%i.obj.back", OBJ_PREFIX, zone);

  if(!(new_file = fopen(new_fname, "w")))
    return 1;

  for(vobj = zone * 100; vobj <= zone_table[rzone].top; vobj++) {

    if((i = real_object(vobj)) >= 0) {
      if(obj_index[i].deleted == FALSE) {
	if(fprintf(new_file, "#%i\n", vobj) < 0) {
	  fclose(new_file);
	  return 1;
	}
	fprintf(new_file, "%s~\n", CHECK_NULL(obj_proto[i].name));
	fprintf(new_file, "%s~\n", CHECK_NULL(obj_proto[i].short_description));
	fprintf(new_file, "%s~\n", CHECK_NULL(obj_proto[i].description));
	fprintf(new_file, "%s~\n", CHECK_NULL(obj_proto[i].action_description));
	*extras = '\0';
	*wears = '\0';
	if(obj_proto[i].obj_flags.extra_flags == 0) {
	  sprintf(extras, "0");
	} else
	    sprintbits(GET_OBJ_EXTRA(obj_proto + i), extras);
	if(obj_proto[i].obj_flags.wear_flags == 0) {
	  sprintf(wears, "0");
	} else
	    sprintbits(GET_OBJ_WEAR(obj_proto + i), wears);
	fprintf(new_file, "%i %s %s\n", 
	  obj_proto[i].obj_flags.type_flag,
	  extras, wears);
	fprintf(new_file, "%i %i %i %i\n", obj_proto[i].obj_flags.value[0],
	  obj_proto[i].obj_flags.value[1], 
	  obj_proto[i].obj_flags.value[2],
	  obj_proto[i].obj_flags.value[3]);
	fprintf(new_file, "%i %i %i\n", obj_proto[i].obj_flags.weight,
	  obj_proto[i].obj_flags.cost, 
	  obj_proto[i].obj_flags.cost_per_day);
	if(obj_proto[i].ex_description) {
	  for(desc = obj_proto[i].ex_description; desc &&
	    str_cmp(desc->keyword, temp2 ? temp2 : "NULL"); temp2 =
	    desc->keyword, desc = desc->next) {
	    fprintf(new_file, "E\n%s~\n", CHECK_NULL(desc->keyword));

	    temp = str_dup(CHECK_NULL(desc->description));
	    kill_ems(temp);
	    fprintf(new_file, "%s~\n", temp);
	    free(temp);

	  }
	}
	for(j = 0; j < MAX_OBJ_AFFECT; j++) {
	  if(obj_proto[i].affected[j].location > 0)
	    fprintf(new_file, "A\n%i %i\n", obj_proto[i].affected[j].location,
	      obj_proto[i].affected[j].modifier);
	}
      }
    }
  }

  fprintf(new_file, "$\n");
  fclose(new_file);
  remove(backname);
  rename(new_fname, backname);
  return 0;
}


int copy_object(struct obj_data * dest, struct obj_data * src)
{ 
  int j;
  struct extra_descr_data *desc, *desc2;

  if(!src || !dest) {
    log("SYSERR:  copy_object: source or destination object empty");
    return 1;
  }

  if(dest->name) free(dest->name);
  if(dest->short_description) free(dest->short_description);
  if(dest->description) free(dest->description);
  if(dest->action_description) free(dest->action_description);

  dest->name = str_dup(CHECK_NULL(src->name));
  dest->short_description = str_dup(CHECK_NULL(src->short_description));
  dest->description = str_dup(CHECK_NULL(src->description));
  dest->action_description = str_dup(CHECK_NULL(src->action_description));
  dest->obj_flags.type_flag = src->obj_flags.type_flag;
  dest->obj_flags.extra_flags = src->obj_flags.extra_flags;
  dest->obj_flags.wear_flags = src->obj_flags.wear_flags;
  dest->obj_flags.value[0] = src->obj_flags.value[0];
  dest->obj_flags.value[1] = src->obj_flags.value[1];
  dest->obj_flags.value[2] = src->obj_flags.value[2];
  dest->obj_flags.value[3] = src->obj_flags.value[3];
  dest->obj_flags.weight = src->obj_flags.weight;
  dest->obj_flags.cost = src->obj_flags.cost;
  dest->obj_flags.cost_per_day = src->obj_flags.cost_per_day;

  if(dest->ex_description) {
    for(desc = dest->ex_description; desc; desc2 = desc, desc = 
	desc->next, free(desc2)) {
      if(desc->keyword) free(desc->keyword);
      if(desc->description) free(desc->description);
    }
    dest->ex_description = NULL;
  }

  if(src->ex_description) {
    for(desc = src->ex_description; desc; desc = desc->next) {
      CREATE(desc2, struct extra_descr_data, 1);
      desc2->keyword = str_dup(CHECK_NULL(desc->keyword));
      desc2->description = str_dup(CHECK_NULL(desc->description));
      desc2->next = dest->ex_description;
      dest->ex_description = desc2;
    }
  } else dest->ex_description = NULL;
  for(j = 0; j < MAX_OBJ_AFFECT; j++) {
    dest->affected[j].location = src->affected[j].location;
    dest->affected[j].modifier = src->affected[j].modifier;
  }

  return 0;
}

int copy_room(sh_int dest, sh_int src)
{ 
  if(src < 0 || dest < 0) {
    log("SYSERR:  copy_room: source or destination room nonexistant");
    return 1;
  }

  if(world[dest].name) free(world[dest].name);
  if(world[dest].description) free(world[dest].description);

  world[dest].name = str_dup(CHECK_NULL(world[src].name));
  world[dest].description = str_dup(CHECK_NULL(world[src].description));
  world[dest].room_flags = world[src].room_flags;
  world[dest].sector_type = world[src].sector_type;

  return 0;
}

int copy_mobile(struct char_data * dest, struct char_data * src)
{
  if(!src || !dest) {
    log("SYSERR:  copy_mobile: source or destination mob empty");
    return 1;
  }

  if(dest->player.name) free(dest->player.name);
  if(dest->player.short_descr) free(dest->player.short_descr);
  if(dest->player.long_descr) free(dest->player.long_descr);
  if(dest->player.description) free(dest->player.description);

  dest->player.name = str_dup(CHECK_NULL(src->player.name));
  dest->player.short_descr = str_dup(CHECK_NULL(src->player.short_descr));
  dest->player.long_descr = str_dup(CHECK_NULL(src->player.long_descr));
  dest->player.description = str_dup(CHECK_NULL(src->player.description));
  MOB_FLAGS(dest) = MOB_FLAGS(src);
  AFF_FLAGS(dest) = AFF_FLAGS(src);
  GET_ALIGNMENT(dest) = GET_ALIGNMENT(src);
  dest->real_abils.str = src->real_abils.str;
  dest->real_abils.intel = src->real_abils.intel;
  dest->real_abils.wis = src->real_abils.wis;
  dest->real_abils.dex = src->real_abils.dex;
  dest->real_abils.con = src->real_abils.con;
  GET_LEVEL(dest) = GET_LEVEL(src);
  dest->points.hitroll = src->points.hitroll;
  dest->points.damroll = src->points.damroll;
  dest->points.armor = src->points.armor;
  dest->points.max_hit = src->points.max_hit;
  dest->points.max_mana = src->points.max_mana;
  dest->points.max_move = src->points.max_move;
  dest->points.hit = src->points.hit;
  dest->points.mana = src->points.mana;
  dest->points.move = src->points.move;
  dest->mob_specials.damnodice = src->mob_specials.damnodice;
  dest->mob_specials.damsizedice = src->mob_specials.damsizedice;
  dest->mob_specials.attack_type = src->mob_specials.attack_type;
  dest->mob_specials.default_pos = src->mob_specials.default_pos;
  dest->char_specials.position = src->char_specials.position;
  GET_GOLD(dest) = GET_GOLD(src);
  GET_EXP(dest) = GET_EXP(src);
  dest->player.sex = src->player.sex;
  dest->player.class = src->player.class;
  dest->player.weight = src->player.weight;
  dest->player.height = src->player.height;
  dest->aff_abils = dest->real_abils;
  dest->player_specials = &dummy_mob;

  return 0;
}


/* yet another attempt to kill the ^M's efficiently */
void kill_ems(char *str)
{
  char *ptr1, *ptr2, *tmp;

  tmp = str;
  ptr1 = str;
  ptr2 = str;

  while(*ptr1) {
    if((*(ptr2++) = *(ptr1++)) == '\r')
      if(*ptr1 == '\r')
	ptr1++;
  }
  *ptr2 = '\0';
}


/*
 * This routine checks all objects in the world and makes sure any
 * string pointers that point to prototype strings will be changed
 * to point to the strings in an object buffer to fix the problem with
 * saving objects that already exist.

   Thanks to Paul Stewart for making this work right.
*/
void fix_obj_strings(struct obj_data * proto, struct obj_data * buffer)
{
  struct obj_data *i;

  for(i = object_list; i; i = i->next) {
    if(i->item_number == proto->item_number) {
      if(i->name && i->name != buffer->name) 
	i->name = buffer->name;
      if(i->description && i->description != buffer->description) 
	i->description = buffer->description;
      if(i->short_description && i->short_description !=
	buffer->short_description) 
	i->short_description = buffer->short_description;
      if(i->action_description && i->action_description !=
	buffer->action_description) 
	i->action_description = buffer->action_description;
      if(i->ex_description && i->ex_description !=
	buffer->ex_description)
	i->ex_description = buffer->ex_description;
    }
  }
}


/*
 * This routine checks all characters in the world and makes sure any
 * string pointers that point to prototype strings will be changed
 * to point to the strings in an mob buffer to fix the problem with
 * saving mobs that already exist.

   Thanks to Paul Stewart for making this work right.
*/
void fix_mob_strings(struct char_data * proto, struct char_data * buffer)
{
  struct char_data *i;

  for(i = character_list; i; i = i->next) {
    if(IS_NPC(i) && i->nr == proto->nr) {
      if(i->player.name && i->player.name != buffer->player.name) 
	i->player.name = buffer->player.name;
      if(i->player.short_descr && i->player.short_descr !=
	buffer->player.short_descr) 
	i->player.short_descr = buffer->player.short_descr;
      if(i->player.long_descr && i->player.long_descr !=
	buffer->player.long_descr) 
	i->player.long_descr = buffer->player.long_descr;
      if(i->player.description && i->player.description !=
	buffer->player.description) 
	i->player.description = buffer->player.description;
    }
  }
}

/* thanks to Daniel Burke for this function */
int create_dir(int room, int dir)
{
   if ((room > top_of_world) || (room < 0)) {
      log("create_dir(): tried to create invalid door");
      return FALSE;
   }
   if (world[room].dir_option[dir])
     return FALSE;
   
   CREATE(world[room].dir_option[dir], struct room_direction_data, 1);
   world[room].dir_option[dir]->to_room = NOWHERE;
   world[room].dir_option[dir]->exit_info = 0;
   world[room].dir_option[dir]->general_description = NULL;
   world[room].dir_option[dir]->keyword = NULL;
   world[room].dir_option[dir]->key = -1;
   
   return TRUE;
   
}


/* thanks to Daniel Burke for this function */
/* This SHOULD trap all problems and completly free the direction */
int free_dir(int room, int dir)
{
   if ((room > top_of_world) || (room < 0)) {
      log("free_dir(): tried to free invalid door");
      return FALSE;
   }
   if ((dir < 0) || (dir >= NUM_OF_DIRS)) {
      log("free_dir(): tried to free invalid door");
      return FALSE;
   }
      
   if (!world[room].dir_option[dir])
     return FALSE;
   
   world[room].dir_option[dir]->to_room = NOWHERE;
   world[room].dir_option[dir]->exit_info = 0;
   if (world[room].dir_option[dir]->general_description)
     free(world[room].dir_option[dir]->general_description);
   if (world[room].dir_option[dir]->keyword)
     free(world[room].dir_option[dir]->keyword);
   world[room].dir_option[dir]->key = -1;
   free(world[room].dir_option[dir]);
   world[room].dir_option[dir] = NULL;
   
   return TRUE;
   
}
#endif
