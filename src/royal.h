/*Royal.h
kingdoms stuff :)
Brian Hartvigsen*/

void save_kings(void);
void init_kings(void);

int parse_kingdom(char arg);

#define MAX_KINGS 50

struct kingdom_rec {
  int	num;
  int	population;
  int	taxes;
  int	war[MAX_KINGS];
  long  treasure;
};

struct kingdom_rec kings[50];
