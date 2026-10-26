/* Finite external-only host initialization check; never pairs controllers. */
#include "hci_external.h"
#include "host.h"
#include "log.h"
#include "util.h"
#include <unistd.h>
int main(void){
 log_open("/data/move-studio/external-host-check.log");
 hci_t transport={0};
 if(!hci_external_open(&transport)){log_line("external transport unavailable; internal radio not attempted");log_close();return 1;}
 host_t*host=host_open(transport,"/data/move-studio/asus-host-check.db",NULL);
 if(!host){log_line("external host setup failed");transport.ops->close(transport.ctx);log_close();return 1;}
 unsigned char address[6];log_line("external host ready; address available=%d; no pairing requested",host_local_address(host,address));
 long end=now_ms()+10000;
 while(now_ms()<end&&!host_transport_lost(host))host_poll(host,10);
 host_close(host);log_line("external host closed; diagnostic complete");log_close();return 0;
}
