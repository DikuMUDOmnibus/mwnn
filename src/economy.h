#ifndef __ECONOMY_H__
#define __ECONOMY_H__

/*
 * define BANK_INVEST if you want to have a stock market type sytem
 * where players can buy and sell shares in hopes of making money.
 * Note that the current formulat for updating market value of shares 
 * is very boring (and surprisingly random!) - you should probably tie
 * it in to the amount of business the shops do.  Or something.  :p
 */
#define BANK_INVEST


/* 
 * Allows players to transfer money directly into aother player's account.  
 * This option requires that both the players in question be logged on to
 * the MUD at the same time.
 */
/*#define BANK_TRANSFER


/*
 * The base starting base of the shares on the market.  This is the 
 * value that shares will cost the first the market comes on line,
 * and if the bank file should ever become corrupt, it will ignore 
 * the bank file and just restart at this amount.
 */
#define SHARE_VALUE  100

#endif /* __ECONOMY_H__ */
