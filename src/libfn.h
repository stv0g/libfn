/**
 * fnordlicht C library header
 *
 * @copyright	2013 Steffen Vogel
 * @license	http://www.gnu.org/licenses/gpl.txt GNU Public License
 * @author	Steffen Vogel <post@steffenvogel.de>
 * @link	http://www.steffenvogel.de
 */
/*
 * This file is part of libfn
 *
 * libfn is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * libfn is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libfn. If not, see <http://www.gnu.org/licenses/>.
 */

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
size_t fn_send(int fd, struct remote_msg_t *msg);
size_t fn_send_mask(int fd, const char *mask, struct remote_msg_t *msg);
size_t fn_sync(int fd);
int fn_get_int(int fd);
uint8_t fn_count_devices(int fd);

#endif
