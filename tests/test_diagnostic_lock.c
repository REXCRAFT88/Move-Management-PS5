/* Pairing must stop before touching host state when diagnostic unlock is absent. */
#include "../src/host.h"
#include <assert.h>
#include <stdio.h>
int __wrap_access(const char *path,int mode){(void)path;(void)mode;return -1;}
int main(void){assert(host_pair(NULL,60)==0);puts("Diagnostic pairing lock passed");return 0;}