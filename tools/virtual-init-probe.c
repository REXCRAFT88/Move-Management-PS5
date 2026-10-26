/* SPDX-License-Identifier: GPL-3.0-or-later
 * Initialize only our Pad client, then restore our process credentials.
 * Does not create a virtual device or open any Bluetooth transport.
 */
#include "ps5_vpad.h"
#include "log.h"
#include <unistd.h>
int main(void) {
    if(!log_open("/data/move-studio/virtual-init-probe.log"))return 1;
    alarm(20);
    log_line("init-only probe begins; no virtual devices or Bluetooth");
    int ok=vpad_init();
    log_line("vpad_init=%d",ok);
    int restored=vpad_creds_restore();
    log_line("credentials restored=%d; complete",restored);
    alarm(0);log_close();return ok&&restored?0:1;
}
