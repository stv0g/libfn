#include <stdio.h>   /* Standard input/output definitions */
#include <termios.h>
#include <fcntl.h>
#include <stdlib.h>
#include <getopt.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "libfn.h"

typedef struct { char * name; char * description; uint8_t cmd; } command_t;
static command_t commands[] = {
	{"fade", "set color/fade to color", REMOTE_CMD_FADE_RGB},
	{"save", "save color to EEPROM", REMOTE_CMD_SAVE_RGB},
	{"modify", "modify current color", REMOTE_CMD_MODIFY_CURRENT},
	{"stop", "stop color changing", REMOTE_CMD_STOP},
	{"start", "start program", REMOTE_CMD_START_PROGRAM},
	{"powerdown", "power down the device", REMOTE_CMD_POWERDOWN},
	{"config", "configure startup & offsets", REMOTE_CMD_CONFIG_OFFSETS},
	{"reset", "reset fnordlichter", REMOTE_CMD_BOOTLOADER},
	{0, 0}	/* stop condition for iterator */
};

static struct option long_options[] = {
	{"delay",	required_argument,	0,		'd'},
	{"step",	required_argument,	0,		's'},
	{"address",	required_argument,	0,		'a'},
	{"mask",	required_argument,	0,		'm'},
	{"slot",	required_argument,	0,		't'},
	{"pause",	required_argument,	0,		'p'},
	{"color",	required_argument,	0,		'c'},
	{"help",	required_argument,	0,		'h'},
	{"port",	required_argument,	0,		'P'},
	{"host",	required_argument,	0,		'H'},
	{"verbose",	no_argument,		0,		'v'},
	{0, 0, 0, 0}  /* stop condition for iterator */
};

void print_cmd(struct remote_msg_t * msg) {
	printf("sending: ");

	int i;
	for (i = 0; i < REMOTE_MSG_LEN; i++) {
		printf("%X", *((uint8_t *) msg+i));
	}
	printf("\n");
}

void usage(char ** argv) {
	printf("usage: %s command [options]\n\n", argv[0]);
	printf("  following commands are available:\n");

	command_t * cp = commands;
	while (cp->name) {
		printf("\t%s%s%s\n", cp->name, (strlen(cp->name) < 7) ? "\t\t" : "\t", cp->description);
		cp++;
	}

	printf("\n");
	printf("  following options are available\n");

	struct option * op = long_options;
	while (op->name) {
		printf("\t--%s,%s-%c\n", op->name, (strlen(op->name) < 5) ? "\t\t" : "\t", op->val);
		op++;
	}
}

struct rgb_color_t parse_color(char * color_def) {
	struct rgb_color_t color;

	if (strlen(color_def) != 6)
		fprintf(stderr, "invalid color definition: %s", color_def);

	sscanf(color_def, "%2x%2x%2x", (unsigned int *) (&color.red), (unsigned int *) (&color.green), (unsigned int *) (&color.blue));

	return color;
}

int main(int argc, char ** argv) {
	/* options */
	uint8_t address = 255;
	uint8_t step = 255;
	uint8_t slot = 0;
	uint8_t delay = 0;
	uint8_t pause = 0;
	char host[255];
	char port[255] = "7970";
	int verbose;

	struct rgb_color_t color;
	struct remote_msg_t msg;
	memset(&msg, 0, sizeof msg);

	/* connection */
	int fd;
	struct addrinfo hints, *res;
	struct termios oldtio;

	/* parse command */
	if (argc <= 1) {
		fprintf(stderr, "command required\n");
		usage(argv);
		exit(-1);
	}

	command_t * cp = commands;
	while (cp->name && strcmp(cp->name, argv[1]) != 0) { cp++; }

	/* parse cli arguments */
 	while (1) {
		/* getopt_long stores the option index here. */
		int option_index = 0;

		int c = getopt_long(argc, argv, "hva:m:d:s:d:p:c:P:H:", long_options, &option_index);

		/* Detect the end of the options. */
		if (c == -1)
			break;

		switch (c) {
			case 'a':
				address = atoi(optarg);
				break;

     			case 'm':
				fprintf(stderr, "not yet implemented\n");
				exit(-1);
				break;

     			case 's':
				step = atoi(optarg);
				break;

     			case 'd':
				delay = atoi(optarg);
				break;

			case 't':
				slot = atoi(optarg);
				break;

			case 'p':
				pause = atoi(optarg);
				break;

			case 'c':
				color = parse_color(optarg);
				break;

			case 'H': {
				char * ps = strrchr(optarg, ':');
				if (ps) { /* with port: "localhost:1234" */
					strcpy(port, optarg + 1);
					strncpy(host, optarg, ps - optarg);
				}
				else { /* without port */
					strcpy(host, optarg);
				}
			}

				memset(&hints, 0, sizeof hints);
				hints.ai_family = AF_UNSPEC;	/* both IPv4 & IPv6 */
				hints.ai_socktype = SOCK_STREAM;

				getaddrinfo(host, port, &hints, &res);

				fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
				connect(fd, res->ai_addr, res->ai_addrlen);

				if (fd < 0) {
					perror(port);
					exit(-1);
				}

				fn_sync(fd);
				usleep(200000);
				break;

			case 'P':
				fd = open(optarg, O_RDWR | O_NOCTTY);
				if (fd < 0) {
					perror(optarg);
					exit(-1);
				}

				oldtio = fn_init(fd);
				break;

			case 'v':
				verbose = 1;
				break;

			case 'h':
			case '?':
				usage(argv);
				exit((c == '?') ? -1 : 0);
		}
	}

	/* check address */
	if (strlen(host) == 0 && address != 255 && address > fn_count_devices(fd)) {
		fprintf(stderr, "device with address %d not found\n", address);
		exit(-1);
	}

	if (verbose) {
		printf("command: %s: %s\n", cp->name, cp->description);
		printf("port: %s\n", port);
		printf("host: %s\n", host);
		printf("address: %d\n", address);
		printf("found %d fnordlichts\n", fn_count_devices(fd));
	}

	switch (cp->cmd) {
		case REMOTE_CMD_FADE_RGB: {
			struct remote_msg_fade_rgb_t * param = (void *) &msg;

			param->step = step;
			param->delay = delay;
			param->color = color;
		}
			break;

		case REMOTE_CMD_MODIFY_CURRENT: {
			struct remote_msg_modify_current_t * param = (void *) &msg;

			struct rgb_color_offset_t ofs = {50,50,50};

			param->step = step;
			param->delay = delay;
			param->rgb = ofs;

		}
			break;

		case REMOTE_CMD_SAVE_RGB: {
			struct remote_msg_save_rgb_t * param = (void *) &msg;

			param->slot = slot;
			param->step = step;
			param->delay = delay;
			param->pause = pause;
			param->color = color;

		}
			break;

		case REMOTE_CMD_START_PROGRAM: {
			struct remote_msg_start_program_t * param = (void *) &msg;

		}
			break;

		case REMOTE_CMD_STOP: {
			struct remote_msg_stop_t * param = (void *) &msg;

		}
			break;

		case REMOTE_CMD_POWERDOWN:
		case REMOTE_CMD_BOOTLOADER:
			break;

		default:
			fprintf(stderr, "unknown subcomand: %s\n", argv[1]);
			usage(argv);
			exit(-1);
	}

	msg.address = address;
	msg.cmd = cp->cmd;

	int i = fn_send(fd, &msg);
	if (i < 0)
		fprintf(stderr, "failed on writing %d bytes to fnordlichts", REMOTE_MSG_LEN);

	if (verbose) {
		print_cmd(&msg);
	}

	if (strlen(host) == 0) {
		/* reset port to old state */
        	tcsetattr(fd, TCSANOW, &oldtio);
	}

	return 0;
}
