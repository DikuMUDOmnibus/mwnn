/* ************************************************************************
*   File: shop.h                                        Part of CircleMUD *
*  Usage: shop file definitions, structures, constants                    *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */


struct shop_buy_data {
   int type;
   char *keywords;
} ;

#define BUY_TYPE(i)		((i).type)
#define BUY_WORD(i)		((i).keywords)


struct shop_data {
   room_vnum vnum;		/* Virtual number of this shop		*/
   obj_vnum *producing;		/* Which item to produce (virtual)	*/
   float profit_buy;		/* Factor to multiply cost with		*/
   float profit_sell;		/* Factor to multiply cost with		*/
   struct shop_buy_data *type;	/* Which items to trade			*/
   char	*no_such_item1;		/* Message if keeper hasn't got an item	*/
   char	*no_such_item2;		/* Message if player hasn't got an item	*/
   char	*missing_cash1;		/* Message if keeper hasn't got cash	*/
   char	*missing_cash2;		/* Message if player hasn't got cash	*/
   char	*do_not_buy;		/* If keeper dosn't buy such things	*/
   char	*message_buy;		/* Message when player buys item	*/
   char	*message_sell;		/* Message when player sells item	*/
   int	 temper1;		/* How does keeper react if no money	*/
   bitvector_t	 bitvector;	/* Can attack? Use bank? Cast here?	*/
   mob_rnum	 keeper;	/* The mobile who owns the shop (rnum)	*/
   int	 with_who;		/* Who does the shop trade with?	*/
   room_rnum *in_room;		/* Where is the shop?			*/
   int	 open1, open2;		/* When does the shop open?		*/
   int	 close1, close2;	/* When does the shop close?		*/
   int	 bankAccount;		/* Store all gold over 15000 (disabled)	*/
   int	 lastsort;		/* How many items are sorted in inven?	*/
   SPECIAL (*func);		/* Secondary spec_proc for shopkeeper	*/
};


#define MAX_TRADE	5	/* List maximums for compatibility	*/
#define MAX_PROD	5	/*	with shops before v3.0		*/
#define VERSION3_TAG	"v3.0"	/* The file has v3.0 shops in it!	*/
#define MAX_SHOP_OBJ	100	/* "Soft" maximum for list maximums	*/


/* Pretty general macros that could be used elsewhere */
#define IS_GOD(ch)		(!IS_NPC(ch) && (GET_LEVEL(ch) >= LVL_GOD))
#define END_OF(buffer)		((buffer) + strlen((buffer)))


/* Possible states for objects trying to be sold */
#define OBJECT_DEAD		0
#define OBJECT_NOTOK		1
#define OBJECT_OK		2
#define OBJECT_NOVAL		3


/* Types of lists to read */
#define LIST_PRODUCE		0
#define LIST_TRADE		1
#define LIST_ROOM		2


/* Whom will we not trade with (bitvector for SHOP_TRADE_WITH()) */
#define TRADE_NOGOOD		(1 << 0)	
#define TRADE_NOEVIL		(1 << 1)
#define TRADE_NONEUTRAL		(1 << 2)
#define TRADE_NOMAGIC_USER	(1 << 3)
#define TRADE_NOTHIEF		(1 << 4)
#define TRADE_NOKNIGHT		(1 << 5)
#define TRADE_NOASSASSIN	(1 << 6)
#define TRADE_NOTRACKER		(1 << 7)
#define TRADE_NOMERC		(1 << 8)
#define TRADE_NOMERCHANT	(1 << 9)
#define TRADE_NOBARD		(1 << 10)
#define TRADE_NOSPY			(1 << 11)
#define TRADE_NOGUARD		(1 << 12)
#define TRADE_NONONE		(1 << 13)
#define TRADE_NOHUMAN		(1 << 14)
#define TRADE_NOELF			(1 << 15)
#define TRADE_NODWARF		(1 << 16)
#define TRADE_NODRACO		(1 << 17)
#define TRADE_NOWERE		(1 << 18)
#define TRADE_NOVAMPYRE		(1 << 19)
#define TRADE_NOTROLL		(1 << 20)
#define TRADE_NOARIEL		(1 << 21)
#define TRADE_NOFAIRY		(1 << 22)
#define TRADE_NOMINOTAUR	(1 << 23)
#define TRADE_NOGIANT		(1 << 24)
#define TRADE_NOGNOME		(1 << 25)
#define TRADE_NOGHOST		(1 << 26)
#define TRADE_NOGHOUL		(1 << 27)
#define TRADE_NODROW		(1 << 28)
#define TRADE_NOKENDER		(1 << 29)


struct stack_data {
   int data[100];
   int len;
} ;

#define S_DATA(stack, index)	((stack)->data[(index)])
#define S_LEN(stack)		((stack)->len)


/* Which expression type we are now parsing */
#define OPER_OPEN_PAREN		0
#define OPER_CLOSE_PAREN	1
#define OPER_OR			2
#define OPER_AND		3
#define OPER_NOT		4
#define MAX_OPER		4


#define SHOP_NUM(i)		(shop_index[(i)].vnum)
#define SHOP_KEEPER(i)		(shop_index[(i)].keeper)
#define SHOP_OPEN1(i)		(shop_index[(i)].open1)
#define SHOP_CLOSE1(i)		(shop_index[(i)].close1)
#define SHOP_OPEN2(i)		(shop_index[(i)].open2)
#define SHOP_CLOSE2(i)		(shop_index[(i)].close2)
#define SHOP_ROOM(i, num)	(shop_index[(i)].in_room[(num)])
#define SHOP_BUYTYPE(i, num)	(BUY_TYPE(shop_index[(i)].type[(num)]))
#define SHOP_BUYWORD(i, num)	(BUY_WORD(shop_index[(i)].type[(num)]))
#define SHOP_PRODUCT(i, num)	(shop_index[(i)].producing[(num)])
#define SHOP_BANK(i)		(shop_index[(i)].bankAccount)
#define SHOP_BROKE_TEMPER(i)	(shop_index[(i)].temper1)
#define SHOP_BITVECTOR(i)	(shop_index[(i)].bitvector)
#define SHOP_TRADE_WITH(i)	(shop_index[(i)].with_who)
#define SHOP_SORT(i)		(shop_index[(i)].lastsort)
#define SHOP_BUYPROFIT(i)	(shop_index[(i)].profit_buy)
#define SHOP_SELLPROFIT(i)	(shop_index[(i)].profit_sell)
#define SHOP_FUNC(i)		(shop_index[(i)].func)

#define NOTRADE_GOOD(i)		(IS_SET(SHOP_TRADE_WITH((i)), TRADE_NOGOOD))
#define NOTRADE_EVIL(i)		(IS_SET(SHOP_TRADE_WITH((i)), TRADE_NOEVIL))
#define NOTRADE_NEUTRAL(i)	(IS_SET(SHOP_TRADE_WITH((i)), TRADE_NONEUTRAL))
#define NOTRADE_MAGIC_USER(i)	(IS_SET(SHOP_TRADE_WITH((i)), TRADE_NOMAGIC_USER))
#define NOTRADE_MERC(i)	    (IS_SET(SHOP_TRADE_WITH((i)), TRADE_NOMERC))
#define NOTRADE_BARD(i)		(IS_SET(SHOP_TRADE_WITH((i)), TRADE_NOBARD))
#define NOTRADE_MERCHANT(i)	(IS_SET(SHOP_TRADE_WITH((i)), TRADE_NOMERCHANT))
#define NOTRADE_THIEF(i)	(IS_SET(SHOP_TRADE_WITH((i)), TRADE_NOTHIEF))
#define NOTRADE_KNIGHT(i)	(IS_SET(SHOP_TRADE_WITH((i)), TRADE_NOKNIGHT))
#define NOTRADE_ASSASSIN(i)	(IS_SET(SHOP_TRADE_WITH((i)), TRADE_NOASSASSIN))
#define NOTRADE_TRACKER(i)	(IS_SET(SHOP_TRADE_WITH((i)), TRADE_NOTRACKER))
#define NOTRADE_SPY(i)		(IS_SET(SHOP_TRADE_WITH((i)), TRADE_NOSPY))
#define NOTRADE_GUARD(i)	(IS_SET(SHOP_TRADE_WITH((i)), TRADE_NOGUARD))
#define NOTRADE_NONE(i)		(IS_SET(SHOP_TRADE_WITH((i)), TRADE_NONONE))
#define NOTRADE_HUMAN(i)	(IS_SET(SHOP_TRADE_WITH((i)), TRADE_NOHUMAN))
#define NOTRADE_DWARF(i)	(IS_SET(SHOP_TRADE_WITH((i)), TRADE_NODWARF))
#define NOTRADE_ELF(i)		(IS_SET(SHOP_TRADE_WITH((i)), TRADE_NOELF))
#define NOTRADE_DRACO(i)	(IS_SET(SHOP_TRADE_WITH((i)), TRADE_NODRACO))
#define NOTRADE_WERE(i)		(IS_SET(SHOP_TRADE_WITH((i)), TRADE_NOWERE))
#define NOTRADE_VAMPYRE(i)	(IS_SET(SHOP_TRADE_WITH((i)), TRADE_NOVAMPYRE))
#define NOTRADE_TROLL(i)	(IS_SET(SHOP_TRADE_WITH((i)), TRADE_NOTROLL))
#define NOTRADE_ARIEL(i)	(IS_SET(SHOP_TRADE_WITH((i)), TRADE_NOARIEL))
#define NOTRADE_FAIRY(i)	(IS_SET(SHOP_TRADE_WITH((i)), TRADE_NOFAIRY))
#define NOTRADE_MINOTAUR(i)	(IS_SET(SHOP_TRADE_WITH((i)), TRADE_NOMINOTAUR))
#define NOTRADE_GIANT(i)	(IS_SET(SHOP_TRADE_WITH((i)), TRADE_NOGIANT))
#define NOTRADE_GNOME(i)	(IS_SET(SHOP_TRADE_WITH((i)), TRADE_NOGNOME))
#define NOTRADE_GHOST(i)	(IS_SET(SHOP_TRADE_WITH((i)), TRADE_NOGHOST))
#define NOTRADE_GHOUL(i)	(IS_SET(SHOP_TRADE_WITH((i)), TRADE_NOGHOUL))
#define NOTRADE_DROW(i)		(IS_SET(SHOP_TRADE_WITH((i)), TRADE_NODROW))
#define NOTRADE_KENDER(i)	(IS_SET(SHOP_TRADE_WITH((i)), TRADE_NOKENDER))

#define	WILL_START_FIGHT	1
#define WILL_BANK_MONEY		2

#define SHOP_KILL_CHARS(i)	(IS_SET(SHOP_BITVECTOR(i), WILL_START_FIGHT))
#define SHOP_USES_BANK(i)	(IS_SET(SHOP_BITVECTOR(i), WILL_BANK_MONEY))

#define MIN_OUTSIDE_BANK	5000
#define MAX_OUTSIDE_BANK	15000

#define MSG_NOT_OPEN_YET	"Come back later!"
#define MSG_NOT_REOPEN_YET	"Sorry, we have closed, but come back later."
#define MSG_CLOSED_FOR_DAY	"Sorry, come back tomorrow."
#define MSG_NO_STEAL_HERE	"$n is a bloody thief!"
#define MSG_NO_SEE_CHAR		"I don't trade with someone I can't see!"
#define MSG_NO_SELL_ALIGN	"Get out of here before I call the guards!"
#define MSG_NO_SELL_CLASS	"We don't serve your kind here!"
#define MSG_NO_SELL_RACE	"Get out of here, we don't serve your kind!"
#define MSG_NO_USED_WANDSTAFF	"I don't buy used up wands or staves!"
#define MSG_CANT_KILL_KEEPER	"Get out of here before I call the guards!"

