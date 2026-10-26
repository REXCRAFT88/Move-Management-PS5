/* SPDX-License-Identifier: GPL-3.0-or-later
 * Own-process passive Bluetooth event subscription, bounded to sixty seconds.
 * No pairing, device registration, disconnect, feature or output reports.
 */
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include <unistd.h>
#include <ps5/kernel.h>
extern int sceKernelLoadStartModule(const char*,size_t,const void*,uint32_t,void*,int*);
static unsigned char samples[256][104];
static _Atomic unsigned total,stored;
static void event_callback(const void *events,int count,void *arg){
 (void)arg;if(!events||count<1||count>16)return;
 atomic_fetch_add(&total,(unsigned)count);
 for(int i=0;i<count;i++){
  unsigned n=atomic_load(&stored);if(n>=256)return;
  memcpy(samples[n],(const unsigned char*)events+(size_t)i*104,104);
  atomic_store(&stored,n+1);
 }
}
int main(void){
 FILE*f=fopen("/data/move-studio/native-input-subscription.log","w");if(!f)return 1;setvbuf(f,NULL,_IOLBF,0);alarm(75);
 int start=0;int h=sceKernelLoadStartModule("/system/common/lib/libSceBluetoothHid.sprx",0,NULL,0,NULL,&start);
 fprintf(f,"module=%d start=%d\n",h,start);if(h<0)goto done;
 void*lib=dlopen("/system/common/lib/libSceBluetoothHid.sprx",RTLD_LAZY);if(!lib)goto done;
 int(*init)(void)=dlsym(lib,"sceBluetoothHidInit");
 int(*reg)(void(*)(const void*,int,void*),void*,const void*)=dlsym(lib,"sceBluetoothHidRegisterCallback");
 int(*unreg)(void)=dlsym(lib,"sceBluetoothHidUnregisterCallback");
 intptr_t base=kernel_dynlib_mapbase_addr(-1,(unsigned)h);if(base<=0||(uintptr_t)init!=(uintptr_t)base+0x1c0||(uintptr_t)reg!=(uintptr_t)base+0x2f0||(uintptr_t)unreg!=(uintptr_t)base+0x740){fputs("ABI gate failed\n",f);goto done;}
 int rc=init();fprintf(f,"init=%#x\n",(unsigned)rc);if(rc)goto done;
 rc=reg(event_callback,NULL,NULL);fprintf(f,"register=%#x\n",(unsigned)rc);if(rc)goto done;
 for(unsigned i=0;i<600;i++)usleep(100000);
 rc=unreg();fprintf(f,"unregister=%#x\n",(unsigned)rc);usleep(100000);
 unsigned n=atomic_load(&stored);fprintf(f,"total=%u stored=%u\n",atomic_load(&total),n);
 for(unsigned i=0;i<n;i++){
  /* Omit device address bytes0..7 from human-readable output. */
  fprintf(f,"event=%u metadata=",i);for(unsigned j=8;j<24;j++)fprintf(f,"%02x",samples[i][j]);
  fputs(" payload=",f);for(unsigned j=24;j<104;j++)fprintf(f,"%02x",samples[i][j]);fputc('\n',f);
 }
 done:alarm(0);fputs("complete\n",f);fclose(f);return 0;
}
