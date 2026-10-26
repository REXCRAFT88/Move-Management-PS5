/* SPDX-License-Identifier: GPL-3.0-or-later
 * One neutral virtual DualSense, identified through the existing conservative
 * device log path, then removed. No physical/radio input or shell patching.
 */
#include "ps5_vpad.h"
#include "log.h"
#include "util.h"
#include <unistd.h>
int main(void) {
    if(!log_open("/data/move-studio/virtual-device-probe.log"))return 1;
    alarm(20);
    int ok=vpad_init(),live=0;
    if(ok&&vpad_add(0)) {
        long end=now_ms()+6000;
        pad_state neutral;pad_state_reset(&neutral);
        while(now_ms()<end) {
            vpad_poll(now_ms());
            if(vpad_live(0)){live=1;vpad_update(0,&neutral);}
            usleep(10000);
        }
        vpad_remove(0);
    }
    log_line("init=%d live=%d credentials_restored=%d; complete",ok,live,vpad_creds_restore());
    alarm(0);log_close();return live?0:1;
}
