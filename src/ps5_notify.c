/* Diagnostic notifications without importing the virtual-pad subsystem. */
#include "ps5_vpad.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
extern int sceKernelSendNotificationRequest(int,void*,size_t,int);
void notify(const char*fmt,...){
 struct{char reserved[45];char message[3075];} req={0};va_list ap;
 va_start(ap,fmt);vsnprintf(req.message,sizeof(req.message),fmt,ap);va_end(ap);
 sceKernelSendNotificationRequest(0,&req,sizeof(req),0);
}