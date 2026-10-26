/* Finite isolation: only open/close the previously identified Bluetooth node.
 * No USB ioctls, descriptors, endpoint allocations, reads or HCI commands. */
#include "log.h"
#include "util.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
int main(void) {
 log_open("/data/move-studio/native-bt-file-only.log");
 log_line("file-only: open O_RDWR /dev/ugen0.2; duration 45 seconds; no USB ioctls");
 int fd = open("/dev/ugen0.2", O_RDWR);
 if (fd < 0) { log_line("open failed errno=%d", errno); log_close(); return 1; }
 log_line("device file open; reconnect test window active");
 long end = now_ms() + 45000;
 while (now_ms() < end) usleep(10000);
 close(fd); log_line("device file closed; diagnostic complete"); log_close(); return 0;
}
