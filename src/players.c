#define _POSIX_C_SOURCE 200809L
#include "players.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
int players_find(const player_registry*r,const unsigned char*a){if(!r||!a)return -1;for(unsigned i=0;i<r->count;i++)if(!memcmp(r->device[i].addr,a,6))return (int)i;return -1;}
int players_assign(player_registry*r,unsigned i,unsigned p,unsigned role){
 if(!r||i>=r->count||p>=4||role>ROLE_OFF)return 0;
 if(role!=ROLE_OFF&&((r->device[i].pid==0x042f)!=(role==ROLE_NAV)))return 0;
 for(unsigned j=0;role!=ROLE_OFF&&j<r->count;j++)if(i!=j&&r->device[j].player==p&&r->device[j].role==role)return 0;
 /* A player's second hand is either a Nav or a second Move. */
 for(unsigned j=0;role!=ROLE_OFF&&j<r->count;j++)if(i!=j&&r->device[j].player==p&&((role==ROLE_NAV&&r->device[j].role==ROLE_SECOND_MOVE)||(role==ROLE_SECOND_MOVE&&r->device[j].role==ROLE_NAV)))return 0;
 r->device[i].player=p;r->device[i].role=role;return 1;
}
void players_release_offline(player_registry*r,const unsigned char active[PLAYER_DEVICES]){
 if(!r||!active)return;
 for(unsigned i=0;i<r->count;i++)if(!active[i])r->device[i].role=ROLE_OFF;
 for(unsigned p=0;p<4;p++){int primary=0,second=-1;for(unsigned i=0;i<r->count;i++)if(r->device[i].player==p){if(r->device[i].role==ROLE_MOVE)primary=1;if(r->device[i].role==ROLE_SECOND_MOVE)second=(int)i;}if(!primary&&second>=0)r->device[second].role=ROLE_MOVE;}
}
int players_auto_assign(player_registry*r,unsigned index,unsigned player){
 if(!r||index>=r->count||player>=4)return 0;
 player_device old=r->device[index];
 if(old.player==player&&old.role!=ROLE_OFF)return 1;
 player_registry next=*r;next.device[index].role=ROLE_OFF;
 unsigned role=old.pid==0x042f?ROLE_NAV:ROLE_MOVE;
 if(old.pid==0x03d5){for(unsigned j=0;j<next.count;j++)if(next.device[j].player==player&&next.device[j].role==ROLE_MOVE)role=ROLE_SECOND_MOVE;}
 if(!players_assign(&next,index,player,role))return 0;
 /* The remaining hand becomes primary when its partner moves away. */
 if(old.role==ROLE_MOVE){for(unsigned j=0;j<next.count;j++)if(next.device[j].player==old.player&&next.device[j].role==ROLE_SECOND_MOVE){next.device[j].role=ROLE_MOVE;break;}}
 *r=next;return 1;
}
int players_register(player_registry*r,const unsigned char*a,unsigned pid){
 static const unsigned char zero[6]={0},ff[6]={255,255,255,255,255,255};
 if(!r||!a||!memcmp(a,zero,6)||!memcmp(a,ff,6)||(pid!=0x03d5&&pid!=0x042f))return -1;
 int found=players_find(r,a);if(found>=0)return r->device[found].pid==pid?found:-1;
 if(r->count>=PLAYER_DEVICES)return -1;
 unsigned i=r->count++;r->device[i]=(player_device){.pid=pid,.role=ROLE_OFF};memcpy(r->device[i].addr,a,6);
 for(unsigned p=0;p<4;p++)if(players_assign(r,i,p,pid==0x042f?ROLE_NAV:ROLE_MOVE))break;
 return (int)i;
}
int players_pair(player_registry*r,unsigned p,int first,int second,unsigned kind){
 if(!r||p>=4||kind>2||first< -1||second< -1||first>=(int)r->count||second>=(int)r->count)return 0;
 if(first<0){if(second>=0||kind)return 0;}else if(r->device[first].pid!=0x03d5)return 0;
 if(kind==0&&second>=0)return 0;
 if(kind&& (first<0||second<0||first==second||r->device[second].pid!=(kind==1?0x042fu:0x03d5u)))return 0;
 player_registry next=*r;
 for(unsigned i=0;i<next.count;i++)if(next.device[i].player==p)next.device[i].role=ROLE_OFF;
 if(first>=0&&!players_assign(&next,(unsigned)first,p,ROLE_MOVE))return 0;
 if(second>=0&&!players_assign(&next,(unsigned)second,p,kind==1?ROLE_NAV:ROLE_SECOND_MOVE))return 0;
 *r=next;return 1;
}
int players_save(const char*path,const player_registry*r){
 if(!path||!r||r->count>PLAYER_DEVICES)return 0;
 char tmp[512];if(snprintf(tmp,sizeof tmp,"%s.tmp",path)>=(int)sizeof tmp)return 0;
 FILE*f=fopen(tmp,"w");if(!f)return 0;int ok=fprintf(f,"version=1\n")>0;
 for(unsigned i=0;i<r->count;i++){const player_device*d=&r->device[i];if(fprintf(f,"%02x%02x%02x%02x%02x%02x %04x %u %u\n",d->addr[0],d->addr[1],d->addr[2],d->addr[3],d->addr[4],d->addr[5],d->pid,d->player,d->role)<0)ok=0;}
 if(fflush(f)||fsync(fileno(f)))ok=0;
 if(fclose(f))ok=0;
 if(ok&&rename(tmp,path))ok=0;
 if(!ok)unlink(tmp);
 return ok;
}
int players_load(const char*path,player_registry*r){
 FILE*f=fopen(path,"r");if(!f)return 0;player_registry next={0};char line[160];int ok=fgets(line,sizeof line,f)&&!strcmp(line,"version=1\n");
 while(ok&&fgets(line,sizeof line,f)){unsigned a[6],pid,p,role;char tail;
 if(sscanf(line,"%2x%2x%2x%2x%2x%2x %x %u %u %c",a,a+1,a+2,a+3,a+4,a+5,&pid,&p,&role,&tail)!=9){ok=0;break;}
 unsigned char addr[6];for(int i=0;i<6;i++)addr[i]=(unsigned char)a[i];
 if(players_find(&next,addr)>=0){ok=0;break;}
 int index=players_register(&next,addr,pid);if(index<0||!players_assign(&next,(unsigned)index,p,role)){ok=0;break;}
 }if(ferror(f))ok=0;fclose(f);if(ok)*r=next;return ok;
}
