/* SPDX-License-Identifier: GPL-3.0-or-later
 * Own-process export availability only. No controller API calls, radio reads,
 * virtual device creation, system process patches, or pairing changes.
 */
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#ifdef CODE_METADATA
#include <ps5/kernel.h>
#include <string.h>
#endif
extern int sceKernelLoadStartModule(const char*,size_t,const void*,uint32_t,void*,int*);
static void inventory(FILE *f,const char *path,const char *const *names) {
    int start=0;
    int h=sceKernelLoadStartModule(path,0,NULL,0,NULL,&start);
    fprintf(f,"module=%s loaded=%d start=%#x\n",path,h>=0,(unsigned)start);
    if(h<0)return;
    void *lib=dlopen(path,RTLD_LAZY);
    if(!lib){fputs("lookup unavailable\n",f);return;}
    for(unsigned i=0;names[i];i++) {
        void *symbol=dlsym(lib,names[i]);
        fprintf(f,"%s present=%d\n",names[i],symbol!=NULL);
#ifdef CODE_METADATA
        /* Bounded own-process text metadata. Never invoke the unknown ABI.
         * Do not cross a page boundary or follow instruction pointers. */
        if(symbol&&(!strcmp(names[i],"scePadSetControllerTypeLeonardo")||
           !strcmp(names[i],"scePadGetControllerTypeLeonardo")||
           !strcmp(names[i],"scePadSetVirtualDeviceComponentInformation")||
           !strcmp(names[i],"scePadVirtualDeviceAddDevice"))) {
            uintptr_t address=(uintptr_t)symbol;
            unsigned char bytes[192];size_t n=sizeof bytes;
            size_t remain=4096-(address&4095);if(n>remain)n=remain;
            if(!kernel_proc_copyout(-1,(intptr_t)address,bytes,n)) {
                fprintf(f,"code %s ",names[i]);
                for(size_t j=0;j<n;j++)fprintf(f,"%02x",bytes[j]);
                fputc('\n',f);
            }
        }
#endif
    }
}
int main(void) {
    const char *const pad[]={
        "scePadVirtualDeviceAddDevice","scePadSetVirtualDeviceComponentInformation",
        "scePadGetControllerTypeLeonardo","scePadSetControllerTypeLeonardo",
        "scePadGetProfileLeonardo","scePadSetProfileLeonardo",
        "scePadParseUniqueDataLeonardo","scePadIsCoPilot","scePadSetIsCoPilot",
        "scePadIsMoveConnected","scePadIsMoveReproductionModel",
        "scePadGetSphereRadius","scePadReadForTracker",NULL
    };
    const char *const move[]={
        "sceMoveInit","sceMoveOpen","sceMoveReadStateLatest",
        "sceMoveReadStateRecent","sceMoveGetDeviceInfo",
        "sceMoveSetLightSphere","sceMoveSetVibration",NULL
    };
    FILE *f=fopen("/data/move-studio/output-api-inventory.log","w");
    if(!f)return 1;
    setvbuf(f,NULL,_IOLBF,0);alarm(20);
    inventory(f,"/system/common/lib/libSceMbus.sprx",(const char *const[]){NULL});
    inventory(f,"/system/common/lib/libScePad.sprx",pad);
    inventory(f,"/system/common/lib/libSceMove.sprx",move);
    alarm(0);fputs("complete; availability does not establish ABI or behavior\n",f);
    fclose(f);return 0;
}
