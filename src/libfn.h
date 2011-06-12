#ifndef LIBFN_H
#define LIBFN_H

#include <stdint.h>
#include <termios.h>
#include <string.h>

#include "color.h"
#include "static-programs.h"
#include "remote-proto.h"

#define FN_BAUDRATE B19200
#define FN_MAX_DEVICES 254
#define FN_INT_LINE TIOCM_CTS

struct termios fn_init(int fd);
size_t fn_send(int fd, struct remote_msg_t * msg);
size_t fn_send_mask(int fd, char *mask, struct remote_msg_t *msg);
size_t fn_sync(int fd);
int fn_get_int(int fd);
uint8_t fn_count_devices(int fd);

#endif
