#include "players.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static void test_automatic_roles(void){
 player_registry r={0};unsigned char addr[6]={1,7,8,9,10,11};
 for(unsigned i=0;i<3;i++){addr[0]=(unsigned char)(i+1);assert(players_register(&r,addr,0x03d5)==(int)i);}
 addr[0]=4;assert(players_register(&r,addr,0x042f)==3);
 assert(!players_auto_assign(&r,1,0)); /* P1 already has primary + Nav. */
 assert(players_auto_assign(&r,3,3)); /* Move Nav to P4. */
 assert(players_auto_assign(&r,1,0));assert(r.device[1].role==ROLE_SECOND_MOVE);
 player_registry before=r;assert(!players_auto_assign(&r,2,0));assert(!memcmp(&before,&r,sizeof r));
 assert(players_auto_assign(&r,0,2)); /* P3 has a primary: this becomes second. */
 assert(r.device[0].role==ROLE_SECOND_MOVE&&r.device[1].role==ROLE_MOVE&&r.device[1].player==0);
 before=r;assert(players_auto_assign(&r,1,0));assert(!memcmp(&before,&r,sizeof r));
 assert(players_auto_assign(&r,3,0));assert(r.device[3].role==ROLE_NAV);
 before=r;assert(!players_auto_assign(&r,3,2));assert(!memcmp(&before,&r,sizeof r));
}
static void test_offline_release(void){
 player_registry r={0};unsigned char a[6]={1,2,3,4,5,7};assert(players_register(&r,a,0x03d5)==0);a[0]=2;assert(players_register(&r,a,0x03d5)==1);a[0]=3;assert(players_register(&r,a,0x042f)==2);
 unsigned char active[PLAYER_DEVICES]={1,1,0};players_release_offline(&r,active);assert(r.device[2].role==ROLE_OFF&&r.device[2].player==0);assert(players_auto_assign(&r,1,0));assert(r.device[1].role==ROLE_SECOND_MOVE);
 active[0]=0;players_release_offline(&r,active);assert(r.device[0].role==ROLE_OFF&&r.device[1].role==ROLE_MOVE);memset(active,0,sizeof active);players_release_offline(&r,active);for(unsigned j=0;j<r.count;j++)assert(r.device[j].role==ROLE_OFF);assert(r.device[1].player==0);
}
int main(void){test_automatic_roles();test_offline_release();player_registry r={0},loaded={0};unsigned char a[6]={1,2,3,4,5,6};
 for(unsigned i=0;i<4;i++){a[0]=(unsigned char)(i+1);int j=players_register(&r,a,0x03d5);assert(j==(int)i);assert(r.device[i].player==i&&r.device[i].role==ROLE_MOVE);}
 assert(!players_assign(&r,1,0,ROLE_MOVE));assert(players_assign(&r,1,0,ROLE_SECOND_MOVE));
 a[0]=5;int nav=players_register(&r,a,0x042f);assert(nav==4);assert(!players_assign(&r,4,0,ROLE_NAV));assert(!players_assign(&r,4,1,ROLE_MOVE));
 assert(players_assign(&r,1,1,ROLE_MOVE));assert(players_assign(&r,4,0,ROLE_NAV));
 assert(players_register(&r,a,0x042f)==4);assert(players_register(&r,a,0x03d5)<0);
 assert(players_pair(&r,1,1,4,1));assert(r.device[1].player==1&&r.device[4].player==1&&r.device[4].role==ROLE_NAV);
 player_registry before=r;assert(!players_pair(&r,0,0,0,2));assert(!memcmp(&r,&before,sizeof r));
 assert(players_pair(&r,0,0,2,2));assert(r.device[2].role==ROLE_SECOND_MOVE&&r.device[2].player==0);
 assert(players_save("/tmp/studio-players-test.ini",&r));assert(players_load("/tmp/studio-players-test.ini",&loaded));assert(!memcmp(&r,&loaded,sizeof r));
 FILE*f=fopen("/tmp/studio-players-test.ini","w");assert(f);fputs("version=1\n010203040506 03d5 0 0\n020203040506 03d5 0 0\n",f);fclose(f);assert(!players_load("/tmp/studio-players-test.ini",&loaded));assert(!memcmp(&r,&loaded,sizeof r));unlink("/tmp/studio-players-test.ini");puts("player identity, assignment, conflict and persistence checks passed");return 0;}
