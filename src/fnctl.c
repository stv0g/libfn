/**
 * fnordlicht CLI control frontend for libfn
 *
 * implements the fnordlicht bus protocol
 * @see https://raw.github.com/fd0/fnordlicht/master/doc/PROTOCOL
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
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "libfn.h"

#define DEFAULT_DEVICE "/dev/ttyUSB0"
#define DEFAULT_PORT "7909"

struct command_t {
	char *name;
	char *description;
	uint8_t cmd;
};

enum connection_t {
	RS232,
	NET
};

static struct command_t commands[] = {
	{"fade", "set color/fade to color", REMOTE_CMD_FADE_RGB},
	{"save", "save color to EEPROM", REMOTE_CMD_SAVE_RGB},
	{"modify", "modify current color", REMOTE_CMD_MODIFY_CURRENT},
	{"stop", "stop color changing", REMOTE_CMD_STOP},
	{"start", "start program", REMOTE_CMD_START_PROGRAM},
	{"powerdown", "power down the device", REMOTE_CMD_POWERDOWN},
	{"config", "configure startup & offsets", REMOTE_CMD_CONFIG_OFFSETS},
	{"reset", "reset fnordlichter", REMOTE_CMD_BOOTLOADER},
	{"pullint", "pull down INT line", REMOTE_CMD_PULL_INT},
	{"eeprom", "put sequence to EEPROM", LOCAL_CMD_EEPROM},
	{"count", "count modules on the bus", LOCAL_CMD_COUNT},
	{} /* stop condition for iterator */
};

static struct option long_options[] = {
	{"delay",	required_argument,	0,		'd'},
	{"step",	required_argument,	0,		's'},
	{"address",	required_argument,	0,		'a'},
	{"mask",	required_argument,	0,		'm'},
	{"slot",	required_argument,	0,		'w'},
	{"pause",	required_argument,	0,		'p'},
	{"color",	required_argument,	0,		'c'},
	{"start",	required_argument,	0,		'f'},
	{"end",		required_argument,	0,		't'},
	{"repeat",	required_argument,	0,		'r'},
	{"help",	no_argument,		0,		'h'},
	{"port",	required_argument,	0,		'P'},
	{"host",	required_argument,	0,		'H'},
	{"filename",	required_argument,	0,		'F'},
	{"verbose",	no_argument,		0,		'v'},
	{} /* stop condition for iterator */
};

static char *long_options_descs[] = {
	"delay between steps when fading (in 10ms)",
	"increment step for fading (1-255)",
	"address of fnordlicht (0-254, 255 for broadcast)",
	"string of '0' and '1' (ex. \"01010101\")",
	"slot in the EEPROM (0-59)",
	"time to wait before fading to next color (in 100ms)",
	"color in RGB hex format without leading # (ex. \"1c1c1c\")",
	"index of the first color (0-254, only for replay)",
	"index of the last color (1-255, included, only for replay)",
	"configure repetition",
	"show this help",
	"serial port or TCP port if --host is specified",
	"hostname or IP of terminal server",
	"with replay eeprom",
	"enable verbose output",
	NULL /* stop condition for iterator */
};


struct rgb_color_t parse_color(char * identifier) {
	struct rgb_color_t color;
	if (strlen(identifier) != 6) {
	        fprintf(stderr, "invalid color definition: %s", identifier);
	}

	sscanf(identifier, "%2X%2X%2X", (unsigned int *) (&color.red), (unsigned int *) (&color.green), (unsigned int *) (&color.blue));
	return color;
}

void print_cmd(struct remote_msg_t * msg) {
	printf("sending: ");

	int i;
	for (i = 0; i < REMOTE_MSG_LEN; i++) {
		printf("%02X", *((uint8_t *) msg+i));
	}
	printf("\n");
}

void usage(char **argv) {
	printf("Usage: fnctl command [options]\n\n");
	printf("Commands:\n");

	struct command_t *cp = commands;
	while (cp->name) {
		printf("  %s%s%s\n", cp->name, (strlen(cp->name) <= 7) ? "\t\t" : "\t", cp->description);
		cp++;
	}

	printf("\n");
	printf("Options:\n");

	struct option *op = long_options;
	char **desc = long_options_descs;
	while (op->name && desc) {
		printf("  -%c, --%s\t%s\n", op->val, op->name, *desc);
		op++;
		desc++;
	}
}

int main(int argc, char ** argv) {
	/* options */
	uint8_t address = 255;
	uint8_t step = 255;
	uint8_t slot = 0;
	uint8_t delay = 0;
	uint8_t pause = 0;

	char mask[255] = "";
	char filename[1024] = "";

	char host[255] = "";
	char port[255] = DEFAULT_DEVICE;
	int verbose = 0;

	struct rgb_color_t color;
	struct remote_msg_t msg;
	union program_params_t params;
	memset(&msg, 0, sizeof(struct remote_msg_t));
	memset(&params, 0, sizeof(union program_params_t));

	/* connection */
	enum connection_t con_mode = RS232;
	int fd;
	struct addrinfo hints, *res;
	struct termios oldtio;

	/* parse command */
	if (argc <= 1) {
		fprintf(stderr, "command required\n");
		usage(argv);
		exit(EXIT_FAILURE);
	}

	struct command_t *cp = commands;
	while (cp->name && strcmp(cp->name, argv[1]) != 0) {
		cp++;
	}

	/* parse cli arguments */
 	while (1) {
		/* getopt_long stores the option index here. */
		int option_index = 0;

		int c = getopt_long(argc, argv, "hva:m:f:d:t:s:f:w:r:d:p:c:P:H:F:", long_options, &option_index);

		/* detect the end of the options. */
		if (c == -1) break;

		switch (c) {
			case 'a':
				address = atoi(optarg);
				break;

     			case 'm':
				strcpy(mask, optarg);
				break;

     			case 's':
				step = atoi(optarg);
				break;

     			case 'd':
				delay = atoi(optarg);
				break;

			case 'w':
				slot = atoi(optarg);
				break;

			case 'p':
				pause = atoi(optarg);
				break;

			case 'c':
				color = parse_color(optarg);
				break;

			case 'f':
				params.replay.start = atoi(optarg);
				break;

			case 't':
				params.replay.end = atoi(optarg);
				break;

			case 'r':
				if (strcmp("none", optarg) == 0) {
					params.replay.repeat = REPEAT_NONE;
				}
				else if (strcmp("start", optarg) == 0) {
					params.replay.repeat = REPEAT_START;
				}
				else if (strcmp("reverse", optarg) == 0) {
					params.replay.repeat = REPEAT_REVERSE;
				}
				else {
					fprintf(stderr, "invalid --repeat value: %s\n", optarg);
					exit(EXIT_FAILURE);
				}
				break;

			case 'H': {
				char *ps = strrchr(optarg, ':');
				if (ps) { /* with port: "localhost:1234" */
					strcpy(port, optarg + 1);
					strncpy(host, optarg, ps - optarg);
				}
				else { /* without port, use default */
					strcpy(port, DEFAULT_PORT);
					strcpy(host, optarg);
				}
				con_mode = NET;
				break;
			}

			case 'P':
				strcpy(port, optarg);
				break;

			case 'F':
				strcpy(filename, optarg);
				break;

			case 'v':
				verbose = 1;
				break;

			case 'h':
			case '?':
				usage(argv);
				exit((c == '?') ? EXIT_FAILURE : EXIT_SUCCESS);
		}
	}

	/* connect to fnordlichter */
	if (con_mode == NET) {
		if (verbose) printf("connect via net: %s:%s\n", host, port);
		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC;	/* both IPv4 & IPv6 */
		hints.ai_socktype = SOCK_STREAM;

		getaddrinfo(host, port, &hints, &res);

		fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		connect(fd, res->ai_addr, res->ai_addrlen);
		if (fd < 0) {
			perror(host);
			exit(EXIT_FAILURE);
		}
	}
	else {
		if (verbose) printf("connect via rs232: %s\n", port);
		fd = open(port, O_RDWR | O_NOCTTY);
		if (fd < 0) {
			perror(port);
			exit(EXIT_FAILURE);
		}
		oldtio = fn_init(fd);
	}

	fn_sync(fd);
	usleep(25000); /* sleeping for 25ms */

	/* check address */
	if (address > FN_MAX_DEVICES+1) {
		fprintf(stderr, "sorry, the fnordlicht bus can't address more the %d devices\n", FN_MAX_DEVICES);
		exit(EXIT_FAILURE);
	}

	if (verbose) printf("command: %s (%s)\n", cp->name, cp->description);

	switch (cp->cmd) {
		/* remote commands */
		case REMOTE_CMD_FADE_RGB:
			msg.fade_rgb.step = step;
			msg.fade_rgb.delay = delay;
			msg.fade_rgb.color = color;
			break;

		case REMOTE_CMD_MODIFY_CURRENT: {
			struct rgb_color_offset_t ofs = { { {50, 50, 50} } };

			msg.modify_current.step = step;
			msg.modify_current.delay = delay;
			msg.modify_current.rgb = ofs;
			break;
		}

		case REMOTE_CMD_SAVE_RGB:
			msg.save_rgb.slot = slot;
			msg.save_rgb.step = step;
			msg.save_rgb.delay = delay;
			msg.save_rgb.pause = pause;
			msg.save_rgb.color = color;
			break;

		case REMOTE_CMD_START_PROGRAM:
			msg.start_program.script = 2;
			msg.start_program.params = params;
			break;

		case REMOTE_CMD_PULL_INT:
			msg.pull_int.delay = 5;
			break;

		/* no special parameters */
		case REMOTE_CMD_STOP:
		case REMOTE_CMD_POWERDOWN:
		case REMOTE_CMD_BOOTLOADER:
			break;

		/* local commands */
		case LOCAL_CMD_COUNT:
			printf("%d\n", fn_count_devices(fd));
			break;

		case LOCAL_CMD_EEPROM: {
			FILE *eeprom_file = fopen(filename, "r");
			char row[1024];

			if (eeprom_file == NULL) {
				perror ("error opening eeprom file");
				exit(EXIT_FAILURE);
			}

			while(!feof(eeprom_file)) {
				if (fgets(row, 1024, eeprom_file) && *row != '#') { /* ignore comments */
					struct remote_msg_t msg;
					memset(&msg, 0, sizeof msg);

					unsigned int slot, address, red, green, blue, step, delay, pause;
					sscanf(row, "%u;%u;%2x%2x%2x;%u;%u;%u", &address, &slot, &red, &green, &blue, &step, &delay, &pause);

					/* validate and set settings */
					msg.cmd = REMOTE_CMD_SAVE_RGB;
					msg.address = (address > 255) ? 255 : address;
					msg.save_rgb.slot = (slot > 255) ? 255 : slot;
					msg.save_rgb.color.red = (red > 255) ? 255 : red;
					msg.save_rgb.color.green = (green > 255) ? 255 : green;
					msg.save_rgb.color.blue = (blue > 255) ? 255 : blue;
					msg.save_rgb.step = (step > 255) ? 255 : step;
					msg.save_rgb.delay = (delay > 255) ? 255 : delay;
					msg.save_rgb.pause = (pause > 65535) ? 65535 : pause;

					int p = fn_send(fd, &msg);
					if (verbose) print_cmd(&msg);
					if (p < 0) {
						fprintf(stderr, "failed on writing %d bytes to fnordlichts", REMOTE_MSG_LEN);
						exit(EXIT_FAILURE);
					}
				}
			}
			break;
		}

		default:
			fprintf(stderr, "unknown subcomand: %s\n", argv[1]);
			usage(argv);
			exit(EXIT_FAILURE);
	}

	/* send remote commands to bus */
	if (cp->cmd < 0xA0) {
		msg.cmd = cp->cmd;

		if (strlen(mask)) { /* use mask */
			if (verbose) printf("sending to mask: %s\n", mask);
			int p = fn_send_mask(fd, mask, &msg);
			if (p < 0) {
				fprintf(stderr, "failed on writing %d bytes to fnordlichts", REMOTE_MSG_LEN);
				exit(EXIT_FAILURE);
			}
			else if (p == 0) {
				fprintf(stderr, "invalid mask! only '0' and '1' are allowed\n");
				exit(EXIT_FAILURE);
			}
		}
		else { /* use single module or broadcast */
			if (verbose) printf("address: %d\n", address);
			msg.address = address;

			int p = fn_send(fd, &msg);
			if (verbose) {
				print_cmd(&msg);
				printf("sent %i bytes to fnordlichts\n", p);
			}
			if (p < 0) {
				fprintf(stderr, "failed on writing %d bytes to fnordlichts", REMOTE_MSG_LEN);
				exit(EXIT_FAILURE);
			}
		}
	}

	/* reset port to old state */
	if (con_mode == RS232) tcsetattr(fd, TCSANOW, &oldtio);

	return EXIT_SUCCESS;
}
