/**
 * fnordlicht flash utility
 *
 * flash your fnordlichts via rs232 and the bootloader
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

#include <stdio.h>
#include <stdlib.h>

#include "libfn.h"

#define MAX_SIZE (0x2000 - 0x400) /* 8kB - 512 words for bootloader (ATmega8) */



void usage() {
	fprintf(stderr, "usage: fnflash SERIAL-PORT FIRMWARE [ADDRESS]\n");
}

int main(int argc, char *argv[]) {
	int fn_fd, fw_fd;
	size_t fw_size;
	struct remote_msg_t fn_msg;

	if (argc < 3 || argc > 4) {
		usage();
		return EXIT_FAILURE;
	}

	fn_msg.addr = (argc == 4) ? atoi(argv[3]) : REMOTE_ADDR_BROADCAST;

	/* connect fnordlichts */
	fn_fd = open(argv[1], O_RDWR | O_NOCTTY);
	if (fn_fd < 0) {
		perror(argv[0]);
		return EXIT_FAILURE;
	}

	fn_init(fn_fd);
	fn_sync(fn_fd);

	/* open firmware file */
	fw_fd = open(argv[2], O_RDONLY);
	if (fw_fd < 0) {
		perror(argv[0]);
		return EXIT_FAILURE;
	}

	/* check length */
	fw_size = lseek(fw_fd, 0, SEEK_END);
	if (fw_size < 0 || fw_size > MAX_SIZE) {
		fprintf(stderr, "Invalid firmware file size!\n");
		return EXIT_FAILURE;
	}

	/* enter bootloader */
	msg.cmd = REMOTE_CMD_PULL_INT;
	msg.pull_int.delay = 10;
	fn_send(fn_fd, &msg);

	msg.cmd = REMOTE_CMD_BOOTLOADER;
	msg.bootloader.magic[0] = BOOTLOADER_MAGIC_BYTE1;
	msg.bootloader.magic[1] = BOOTLOADER_MAGIC_BYTE2;
	msg.bootloader.magic[2] = BOOTLOADER_MAGIC_BYTE3;
	msg.bootloader.magic[3] = BOOTLOADER_MAGIC_BYTE4;
	fn_send(fn_fd, &msg);

	/* start flashing */
	while () {

		/* get chunk */
		lseek(fw_fd, 0, SEEK_SET);

		/* calc crc16 */

		/* transfer */

		/* flash */
	
	}

	/* start verifying */

	return 0;
}
