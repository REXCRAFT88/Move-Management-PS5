/* SPDX-License-Identifier: GPL-3.0-or-later
 * Own-process module startup only. No radio, virtual device, physical pad,
 * shell injection, pairing, or system-process changes.
 */
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
extern int sceKernelLoadStartModule(const char*,size_t,const void*,uint32_t,void*,int*);
#ifdef DIRECT_CLIENT
extern int sceUserServiceInitialize(void*);
extern int scePadInit(void);
#endif
int main(void) {
    const char *paths[] = {
        "/system/common/lib/libSceUserService.sprx",
        "/system/common/lib/libSceMbus.sprx",
        "/system/common/lib/libScePad.sprx"
    };
    const char *symbols[] = {
        "sceUserServiceInitialize", "sceMbusBindDeviceWithUserId",
        "scePadVirtualDeviceAddDevice"
    };
    FILE *f=fopen("/data/move-studio/virtual-startup-probe.log","w");
    if(!f)return 1;
    setvbuf(f,NULL,_IOLBF,0);alarm(20);
    for(unsigned i=0;i<3;i++) {
        int start=0;
        fprintf(f,"load stage=%u path=%s\n",i,paths[i]);
        int h=sceKernelLoadStartModule(paths[i],0,NULL,0,NULL,&start);
        fprintf(f,"load result=%d start=%#x\n",h,(unsigned)start);
        if(h<0)break;
        fprintf(f,"resolve stage=%u\n",i);
        void *lib=dlopen(paths[i],RTLD_LAZY);
        if(!lib){fputs("lookup handle unavailable\n",f);break;}
        void *symbol=dlsym(lib,symbols[i]);
        fprintf(f,"symbol available=%d\n",symbol!=NULL);
#ifdef INIT_CLIENT
        if(i==0&&symbol) {
            fputs("calling UserServiceInitialize\n",f);
            int (*init)(void*)=symbol;
#ifdef DIRECT_CLIENT
            init=sceUserServiceInitialize;
#endif
            fprintf(f,"UserServiceInitialize=%#x\n",(unsigned)init(NULL));
        }
        if(i==2) {
            int (*init)(void)=dlsym(lib,"scePadInit");
#ifdef DIRECT_CLIENT
            init=scePadInit;
#endif
            if(init){fputs("calling PadInit\n",f);fprintf(f,"PadInit=%#x\n",(unsigned)init());}
        }
#endif
    }
    alarm(0);fputs("complete; no devices created\n",f);fclose(f);return 0;
}
