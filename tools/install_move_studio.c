#include "../src/launcher.h"
#include "icon_data.h"
#include <dlfcn.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
extern uint64_t kernel_get_ucred_authid(pid_t);
extern int32_t kernel_set_ucred_authid(pid_t,uint64_t);
extern int32_t kernel_get_ucred_caps(pid_t,uint8_t[16]);
extern int32_t kernel_set_ucred_caps(pid_t,const uint8_t[16]);
int main(void){
 mkdir("/data/move-studio",0755);FILE*f=fopen("/data/move-studio/install.log","w");if(!f)return 1;setvbuf(f,NULL,_IOLBF,0);
 uint64_t original=kernel_get_ucred_authid(getpid());uint8_t caps[16],full[16],check[16];int raised=0,ok=0;
 if(!original||kernel_get_ucred_caps(getpid(),caps)){fputs("credential_read_failed\n",f);goto done;}
 for(int i=0;i<16;i++)full[i]=255;
 if(kernel_set_ucred_authid(getpid(),UINT64_C(0x3800000000010003))||kernel_set_ucred_caps(getpid(),full)){fputs("credential_raise_failed\n",f);goto restore;}
 raised=1;
 if(kernel_get_ucred_authid(getpid())!=UINT64_C(0x3800000000010003)||kernel_get_ucred_caps(getpid(),check)||memcmp(check,full,16)){fputs("credential_raise_readback_failed\n",f);goto restore;}
 int changed=launcher_write(LAUNCHER_APP_ROOT,studio_icon,sizeof studio_icon);fprintf(f,"title=%s files=%d\n",LAUNCHER_TITLE_ID,changed);if(changed<0)goto restore;
 void*lib=dlopen("/system/common/lib/libSceAppInstUtil.sprx",RTLD_NOW|RTLD_GLOBAL);
 if(!lib){fprintf(f,"appinst_load_failed=%s\n",dlerror());goto restore;}
 int(*init)(void)=dlsym(lib,"sceAppInstUtilInitialize");int(*install)(const char*,const char*,void*)=dlsym(lib,"sceAppInstUtilAppInstallTitleDir");int(*term)(void)=dlsym(lib,"sceAppInstUtilTerminate");
 if(!init||!install||!term){fputs("appinst_exports_missing\n",f);dlclose(lib);goto restore;}
 int rc=init();fprintf(f,"initialize=%#x\n",rc);if(!rc){rc=install(LAUNCHER_TITLE_ID,LAUNCHER_APP_ROOT "/",NULL);fprintf(f,"install=%#x\n",rc);ok=rc==0;term();}dlclose(lib);
restore:
 {int a=kernel_set_ucred_authid(getpid(),original),b=kernel_set_ucred_caps(getpid(),caps);fprintf(f,"credentials_restored=%d raised=%d\n",!a&&!b&&kernel_get_ucred_authid(getpid())==original&&!kernel_get_ucred_caps(getpid(),check)&&!memcmp(check,caps,16),raised);}
done:fprintf(f,"complete=%d url=%s\n",ok,LAUNCHER_URL);fclose(f);return ok?0:1;
}