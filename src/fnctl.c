#include <stdio.h>   /* Standard input/output definitions */
#include <termios.h>
#include <fcntl.h>
#include <stdlib.h>
#include <getopt.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "../lib/libfn.h"

/* local commands (>= 0xA0) */
#define LOCAL_CMD_EEPROM 0xA0
#define LOCAL_CMD_COUNT 0xA1

#define DEFAULT_HOST "localhost"
#define DEFAULT_PORT "7970"
#define DEFAULT_DEVICE "/dev/ttyUSB0"

struct command_t {
	char * name;
	char * description;
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
	{"eeprom", "put sequence to EEPROM", LOCAL_CMD_EEPROM},
	{"count", "count modules on the bus", LOCAL_CMD_COUNT},
	{0}	/* stop condition for iterator */
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
	{"help",	required_argument,	0,		'h'},
	{"port",	required_argument,	0,		'P'},
	{"host",	required_argument,	0,		'H'},
	{"filename",	required_argument,	0,		'F'},
	{"verbose",	no_argument,		0,		'v'},
	{0}  /* stop condition for iterator */
};

struct rgb_color_t parse_color(char * identifier) {
	struct rgb_color_t color;
	if (strlen(identifier) != 6) {
	        fprintf(stderr, "invalid color definition: %s", identifier);
	}

	sscanf(identifier, "%2x%2x%2x", (unsigned int *) (&color.red), (unsigned int *) (&color.green), (unsigned int *) (&color.blue));
	return color;
}

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

	struct command_t * cp = commands;
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

int main(int argc, char ** argv) {
	/* options */
	uint8_t address = 255;
	uint8_t step = 255;
	uint8_t slot = 0;
	uint8_t delay = 0;
	uint8_t pause = 0;

	char mask[254] = "";
	char filename[1024] = "";

	enum connection_t con_mode = RS232;
	char host[255] = DEFAULT_HOST;
	char port[255] = DEFAULT_PORT;
	char device[255] = DEFAULT_DEVICE;
	int verbose = 0;

	struct rgb_color_t color;
	struct remote_msg_t msg;
	union program_params_t params;
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

	struct command_t * cp = commands;
	while (cp->name && strcmp(cp->name, argv[1]) != 0) {
		cp++;
	}

	/* parse cli arguments */
 	while (1) {
		/* getopt_long stores the option index here. */
		int option_index = 0;

		int c = getopt_long(argc, argv, "hva:m:f:d:t:s:f:w:r:d:p:c:P:H:F:", long_options, &option_index);

		/* Detect the end of the options. */
		if (c == -1)
			break;

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
					exit(-1);
				}
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
				exit((c == '?') ? -1 : 0);
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
			exit(-1);
		}
	}
	else {
		if (verbose) printf("connect via rs232: %s\n", device);
		fd = open(device, O_RDWR | O_NOCTTY);
		if (fd < 0) {
			perror(port);
			exit(-1);
		}
		oldtio = fn_init(fd);
	}

	fn_sync(fd);

	/* check address */
	if (address > FN_MAX_DEVICES+1) {
		fprintf(stderr, "sorry, the fnordlicht bus can't address more the %d devices\n", FN_MAX_DEVICES);
		exit(-1);
	}

	if (verbose) printf("command: %s (%s)\n", cp->name, cp->description);

	switch (cp->cmd) {
		/* remote commands */
		case REMOTE_CMD_FADE_RGB: {
			struct remote_msg_fade_rgb_t * cmsg = (void *) &msg;

			cmsg->step = step;
			cmsg->delay = delay;
			cmsg->color = color;

			break;
		}

		case REMOTE_CMD_MODIFY_CURRENT: {
			struct remote_msg_modify_current_t * cmsg = (void *) &msg;

			struct rgb_color_offset_t ofs = {50, 50, 50};

			cmsg->step = step;
			cmsg->delay = delay;
			cmsg->rgb = ofs;

			break;
		}

		case REMOTE_CMD_SAVE_RGB: {
			struct remote_msg_save_rgb_t * cmsg = (void *) &msg;

			cmsg->slot = slot;
			cmsg->step = step;
			cmsg->delay = delay;
			cmsg->pause = pause;
			cmsg->color = color;
			
			break;
		}

		case REMOTE_CMD_START_PROGRAM: {
			struct remote_msg_start_program_t * cmsg = (void *) &msg;
			
			cmsg->script = 2;
			cmsg->params = params;
			
			break;
		}
			
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
				exit(-1);
			}

			while(!feof(eeprom_file)) {
				if (fgets(row, 1024, eeprom_file) && *row != '#') { /* ignore comments */
					struct remote_msg_save_rgb_t msg;
					memset(&msg, 0, sizeof msg);
					
					msg.cmd = REMOTE_CMD_SAVE_RGB;
					
					sscanf(row, "%d;%d;%2x%2x%2x;%d;%d;%d",
						(unsigned int *) &msg.slot,
						(unsigned int *) &msg.address,
						(unsigned int *) (&color.red),
						(unsigned int *) (&color.green),
						(unsigned int *) (&color.blue),
						(unsigned int *) &msg.step,
						(unsigned int *) &msg.delay,
						(unsigned int *) &msg.pause
					);

					int p = fn_send(fd, (struct remote_msg_t *) &msg);
					if (verbose) print_cmd((struct remote_msg_t *) &msg);
					if (p < 0) {
						fprintf(stderr, "failed on writing %d bytes to fnordlichts", REMOTE_MSG_LEN);
						exit(-1);
					}
				}
			}
			break;
		}

		default:
			fprintf(stderr, "unknown subcomand: %s\n", argv[1]);
			usage(argv);
			exit(-1);
	}

	/* send remote commands to bus */
	if (cp->cmd < 0xA0) {
		if (verbose) printf("address: %d\n", address);
		msg.address = address;
		msg.cmd = cp->cmd;

		int c = strlen(mask);
		if (c > 0) {
			if (verbose) printf("sending to mask: %s\n", mask);
			int i;
			for (i = 0; i < c; i++) {
				if (mask[i] == '1') {
					msg.address = i;
					int p = fn_send(fd, &msg);
					if (verbose) print_cmd(&msg);
					if (p < 0) {
						fprintf(stderr, "failed on writing %d bytes to fnordlichts", REMOTE_MSG_LEN);
						exit(-1);
					}
				}
				else if (mask[i] != '0') {
					fprintf(stderr, "invalid mask! only '0' and '1' are allowed\n");
					exit(-1);
				}
			}
		}
		else {
			int p = fn_send(fd, &msg);
			if (verbose) print_cmd(&msg);
			if (p < 0) {
				fprintf(stderr, "failed on writing %d bytes to fnordlichts", REMOTE_MSG_LEN);
				exit(-1);
			}
		}
	}
	
	if (con_mode == RS232) tcsetattr(fd, TCSANOW, &oldtio);	/* reset port to old state */

	return 0;
}
