/*RACES
MAde for alll race dependancies
*/

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "db.h"
#include "comm.h"

int parse_race(char arg);
long find_race_bitvector(char arg);

const char *race_abbrevs[] = {   
  "Hum",                         
  "Elf",                                               
  "Dwa",
  "Dra",
  "Wer",
  "Vam",
  "Trl",
  "Arl",
  "Fai",
  "Min",
  "Gnt",
  "Gno",
  "Gst",
  "Ghl",
  "Dro",
  "Ken",
  "\n"                           
};                               
                                 
const char *pc_race_types[] = {  
  "Human",                       
  "Elf",                                                
  "Dwarf",
  "Draco",
  "Werebeast",
  "Vamprye",
  "Troll",
  "Ariel",
  "Fairy",
  "Minotaur",
  "Giant",
  "Gnome",
  "Ghost",
  "Ghoul"
  "Drow",
  "Kender",
  "\n"                           
};                               

/* The menu for choosing a race in interpreter.c: */ 
const char *race_menu =                              
"\r\n"                                               
"Select a race:\r\n"                                 
"  [H]uman\r\n"                                      
"  [E]lf\r\n"                                        
"  [G]nome\r\n"                                      
"  [D]warf\r\n"
"  dra[C]o\r\n"
"  [W]erebeast\r\n"
"  [V]ampyre\r\n"
"  [T]roll\r\n"
"  [A]riel\r\n"
"  [F]airy\r\n"
"  [M]inotaur\r\n"
"  g[I]ant\r\n"
"  gh[O]st\r\n"
"  gho[U]l\r\n"
"  d[R]ow\r\n"
"  [K]ender\r\n";

/*
 * The code to interpret a race letter (used in interpreter.c when a 
 * new character is selecting a race).                               
 */                                                                  
int parse_race(char arg)                                             
{                                                                    
  arg = LOWER(arg);                                                  
                                                                     
  switch (arg) {                                                     
  case 'h':return RACE_HUMAN;                                                                                                      
  case 'e':return RACE_ELF;                                                                                                         
  case 'g':return RACE_GNOME;                                                                                                        
  case 'd':return RACE_DWARF;                                               
  case 'c':return RACE_DRACO;
  case 'w':return RACE_WERE;
  case 'v':return RACE_VAMPYRE;
  case 't':return RACE_TROLL;
  case 'a':return RACE_ARIEL;
  case 'f':return RACE_FAIRY;
  case 'm':return RACE_MINOTAUR;
  case 'i':return RACE_GIANT;
  case 'o':return RACE_GHOST;
  case 'u':return RACE_GHOUL;
  case 'r':return RACE_DROW;
  case 'k':return RACE_KENDER;
  default:return RACE_OTHER;                                                          
  }                                                                  
}                                                                    

long find_race_bitvector(char arg)                                   
{                                                                    
  arg = LOWER(arg);                                                  
                                                                     
  switch (arg) {                                                     
    case 'h':                                                     
      return (1 << RACE_HUMAN);                                                                            
    case 'e':                                                        
      return (1 << RACE_ELF);                                                                                                              
    case 'g':                                                        
      return (1 << RACE_GNOME);                                                                                                               
    case 'd':                                                        
      return (1 << RACE_DWARF);                                                      
    case 'c':
	return (1 << RACE_DRACO);
  case 'w':
	return (1 << RACE_WERE);
  case 'v':
	return (1 << RACE_VAMPYRE);
  case 't':
	return (1 << RACE_TROLL);
  case 'a':
	return (1 << RACE_ARIEL);
  case 'f':
	return (1 << RACE_FAIRY);
  case 'm':
	return (1 << RACE_MINOTAUR);
  case 'i':
	return (1 << RACE_GIANT);
  case 'o':
	return (1 << RACE_GHOST);
  case 'u':
	return (1 << RACE_GHOUL);
  case 'r':
	return (1 << RACE_DROW);
  case 'k':
	return (1 << RACE_KENDER);

   default:                                                         
      return 0;                                                                                                              
  }                               
}
int invalid_race(struct char_data *ch, struct obj_data *obj) {
  if ((IS_OBJ_STAT(obj, ITEM_ANTI_HUMAN) && IS_HUMAN(ch)) ||
      (IS_OBJ_STAT(obj, ITEM_ANTI_DWARF) && IS_DWARF(ch)) ||
      (IS_OBJ_STAT(obj, ITEM_ANTI_ELF) && IS_ELF(ch)) ||
	  (IS_OBJ_STAT(obj, ITEM_ANTI_GNOME) && IS_GNOME(ch)) ||
	  (IS_OBJ_STAT(obj, ITEM_ANTI_DRACO) && IS_DRACO(ch)) ||
	  (IS_OBJ_STAT(obj, ITEM_ANTI_WERE) && IS_WERE(ch)) ||
	  (IS_OBJ_STAT(obj, ITEM_ANTI_VAMPYRE) && IS_VAMPYRE(ch)) ||
	  (IS_OBJ_STAT(obj, ITEM_ANTI_TROLL) && IS_TROLL(ch)) ||
	  (IS_OBJ_STAT(obj, ITEM_ANTI_ARIEL) && IS_ARIEL(ch)) ||
	  (IS_OBJ_STAT(obj, ITEM_ANTI_FAIRY) && IS_FAIRY(ch)) ||
	  (IS_OBJ_STAT(obj, ITEM_ANTI_MINOTAUR) && IS_MINOTAUR(ch)) ||
	  (IS_OBJ_STAT(obj, ITEM_ANTI_GIANT) && IS_GIANT(ch)) ||
	  (IS_OBJ_STAT(obj, ITEM_ANTI_GHOST) && IS_GHOST(ch)) ||
	  (IS_OBJ_STAT(obj, ITEM_ANTI_GHOUL) && IS_GHOUL(ch)) ||
	  (IS_OBJ_STAT(obj, ITEM_ANTI_DROW) && IS_DROW(ch)) ||
	  (IS_OBJ_STAT(obj, ITEM_ANTI_KENDER) && IS_KENDER(ch)))
    return 1;
  else
    return 0;
}
