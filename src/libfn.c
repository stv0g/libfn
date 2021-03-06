/**
 * fnordlicht C library
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

#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>

#include "libfn.h"

struct termios fn_init(int fd) {
	struct termios oldtio, newtio;

	tcgetattr(fd, &oldtio); /* save current port settings */

	memset(&newtio, 0, sizeof(newtio));
	newtio.c_cflag = CS8 | CLOCAL | CREAD;
	newtio.c_iflag= 0;
	newtio.c_oflag= 0;
	newtio.c_lflag= 0;

	newtio.c_cc[VMIN] = 1;
	newtio.c_cc[VTIME] = 5;

	cfsetispeed(&newtio, FN_BAUDRATE);
	cfsetospeed(&newtio, FN_BAUDRATE);

	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, &newtio);

	return oldtio;
}

size_t fn_send(int fd, struct remote_msg_t *msg) {
	return write(fd, msg, REMOTE_MSG_LEN);
}

size_t fn_send_mask(int fd, const char *mask, struct remote_msg_t *msg) {
	int i, p = 0, c = strlen(mask);
	for (i = 0; i < c; i++) {
		if (mask[i] == '1') {
			msg->address = i;
			int q = fn_send(fd, msg);
			if (q < 0) {
				return q;
			}
			else {
				p += q;
			}
		}
		else if (mask[i] != '0') {
			return -2; /* invalid mask */
		}
	}

	return p;
}

size_t fn_sync(int fd) {
	uint8_t sync[REMOTE_SYNC_LEN+1];
	memset(sync, REMOTE_SYNC_BYTE, REMOTE_SYNC_LEN);
	sync[REMOTE_SYNC_LEN] = 0;	/* address byte */

	return write(fd, sync, REMOTE_SYNC_LEN+1);
}

uint8_t fn_count_devices(int fd) {
	struct remote_msg_t msg;
	memset(&msg, 0, REMOTE_MSG_LEN);

	msg.address = 0;
	msg.cmd = REMOTE_CMD_PULL_INT;
	msg.pull_int.delay = 1; /* 50ms */

	while (1) {
		fn_send(fd, (struct remote_msg_t *) &msg);
		usleep(25000);

		if (fn_get_int(fd)) {
			msg.address++;
			usleep(25000);
		}
		else {
			break;
		}
	}

	return msg.address;
}

int fn_get_int(int fd) {
	int i;
	ioctl (fd, TIOCMGET, &i);

	return i & FN_INT_LINE;
}
