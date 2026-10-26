#ifndef MOVE_PLAYERS_H
#define MOVE_PLAYERS_H
#include <stdint.h>
#define PLAYER_DEVICES 16
#define PHYSICAL_LIMIT 8
enum {ROLE_MOVE=0,ROLE_NAV=1,ROLE_SECOND_MOVE=2,ROLE_OFF=3};
typedef struct {unsigned char addr[6];unsigned pid,player,role;} player_device;
typedef struct {unsigned count;player_device device[PLAYER_DEVICES];} player_registry;
int players_find(const player_registry*,const unsigned char*);
int players_register(player_registry*,const unsigned char*,unsigned);
int players_assign(player_registry*,unsigned,unsigned,unsigned);
void players_release_offline(player_registry*,const unsigned char active[PLAYER_DEVICES]);
int players_auto_assign(player_registry*,unsigned device,unsigned player);
int players_pair(player_registry*,unsigned player,int primary,int secondary,unsigned kind);
int players_load(const char*,player_registry*);
int players_save(const char*,const player_registry*);
#endif
