/**
 * fnordlicht web control frontend
 *
 * simple ajax colorwheel frontend to control fnordlicht's
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
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <microhttpd.h>
#include <time.h>

#include "libfn.h"

volatile bool terminate = false;/* will be set to TRUE in our signal handler */
char *httpd_root;		/* where static HTML content is located */
int httpd_port;			/* TCP port the webserver should listen to */
int fnordlicht_fd;		/* file descriptor for serial port */

struct rgb_color_t parse_color(const char *identifier) {
	struct rgb_color_t color;
	if (strlen(identifier) != 6) {
		fprintf(stderr, "Invalid color definition: %s", identifier);
	}

	unsigned short int red, green, blue;

	sscanf(identifier, "%2hX%2hX%2hX", &red, &green, &blue);

	color.red = red;
	color.green = green;
	color.blue = blue;

	return color;
}

void quit(int sig) {
	terminate = true;
}

const char * get_filename_ext(char *filename) {
	const char *dot = strrchr(filename, '.');

	if (!dot || dot == filename) {
		return NULL;
	}

	return dot + 1;
}

const char * get_content_type(char *filename) {
	const char *ext = get_filename_ext(filename);

	if (strcmp(ext, "html") == 0) return "text/html";
	else if (strcmp(ext, "js") == 0) return "text/javascript";
	else if (strcmp(ext, "css") == 0) return "text/css";
	else if (strcmp(ext, "png") == 0) return "image/png";
	else return "text/plain";
}

void print_cmd(void *data, size_t n) {
	printf("Command sent: ");
	int i;
	for (i = 0; i < n; i++) {
		printf("%02X", *((uint8_t *) data+i));
	}
	printf("\n");
}

int load_file(const char *filename, char **result) {
	int size = 0;
	FILE *f = fopen(filename, "rb");
	if (f == NULL) {
		*result = NULL;
		return -1; /* failed to open file */
	}

	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);

	*result = (char *) malloc(size+1);
	if (*result == NULL) {
		return -3; /* failed to allocate memory */
	}

	if (size != fread(*result, sizeof(char), size, f)) {
		free(*result);
		return -2; /* failed to read file */
	}

	fclose(f);

	(*result)[size] = '\0'; /* zero terminated */

	return size;
}

int handle_request(void *cls, struct MHD_Connection *connection, const char *url, const char *method,
			const char *version, const char *upload_data, size_t *upload_data_size, void **con_cls) {

	static struct rgb_color_t current = {{{0, 0, 0}}}; /* start with black */

	if (strcasecmp(method, "get") == 0) {
		struct MHD_Response *response;
		char *filename = malloc(1024); /* max path length */

		if (strcmp(url+1, "color") == 0) {
			char *color_str = malloc(8);
			snprintf(color_str, 7, "%02x%02x%02x\n", current.red, current.green, current.blue);
			response = MHD_create_response_from_data(strlen(color_str), (void *) color_str, 1, 0);
			MHD_add_response_header(response, "Content-type", "text/plain");
		}
		else if (strcmp(url+1, "count") == 0) {
			char *count_str = malloc(5);
			snprintf(count_str, 4, "%i\n", fn_count_devices(fnordlicht_fd));
			response = MHD_create_response_from_data(strlen(count_str), (void *) count_str, 1, 0);
			MHD_add_response_header(response, "Content-type", "text/plain");
		}
		else { /* get file from disk */
			/* construct filename */
			filename = getcwd(filename, 1024);
			filename = strncat(filename, httpd_root, 1024);
			filename = strncat(filename, (strlen(url) > 1) ? url : "/index.html", 1024);

			/* try to open file */
			char *response_str;
			int resonse_size = load_file(filename, &response_str);
			if (resonse_size > 0) {
				response = MHD_create_response_from_data(resonse_size, (void *) response_str, 1, 0);
				MHD_add_response_header(response, "Content-type", get_content_type(filename));
			}
			else {
				const char *error_str = "<html><body><h2>File not found!</h2></body></html>";
				fprintf(stderr, "Failed to open file: %s\n", filename);
				response = MHD_create_response_from_data(strlen (error_str), (void *) error_str, 0, 0);
			}
		}

		if (response) {
			int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
			MHD_destroy_response(response);
			return ret;
		}
		else {
			return MHD_NO;
		}
	}
	else if (strcasecmp(method, "post") == 0) {
		struct remote_msg_t msg;

		struct MHD_Response *response;
		const char *response_str;

		memset(&msg, 0, sizeof(struct remote_msg_t));

		const char *address = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "address");
		const char *mask = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "mask");

		msg.address = (address) ? atoi(address) : REMOTE_ADDR_BROADCAST;

		if (strcmp(url+1, "fade") == 0) {
			const char *color = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "color");
			const char *step = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "step");
			const char *delay = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "delay");

			if (color == NULL) {
				return MHD_NO;
			}

			current = parse_color(color); /* parse and updating current color */
			printf("Fading to color: #%02x%02x%02x\n", current.red, current.green, current.blue);

			msg.cmd = REMOTE_CMD_FADE_RGB;
			msg.fade_rgb.step = (step) ? atoi(step) : 255;
			msg.fade_rgb.delay = (delay) ? atoi(delay) : 0;
			msg.fade_rgb.color = current;
		}
		else if (strcmp(url+1, "start") == 0) {
			/* parameters */
			const char *script = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "script");

			const char *step = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "step");
			const char *delay = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "delay");
			const char *sleep = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "sleep");

			const char *value = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "value");
			const char *saturation = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "saturation");

			const char *use_address = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "use_address");
			const char *wait_for_fade = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "wait_for_fade");
			const char *min_distance = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "min_distance");

			msg.cmd = REMOTE_CMD_START_PROGRAM;
			msg.start_program.script = (uint8_t) atoi(script);

			switch (atoi(script)) {
				case 0:
					printf("Start program: colorwheel\n");
					msg.start_program.params.colorwheel.hue_start = 0;
					msg.start_program.params.colorwheel.hue_step = 60;
					msg.start_program.params.colorwheel.add_addr = (use_address) ? atoi(use_address) : 1;
					msg.start_program.params.colorwheel.saturation = (saturation) ? atoi(saturation) : 255;
					msg.start_program.params.colorwheel.value = (value) ? atoi(value) : 255;

					msg.start_program.params.colorwheel.fade_step = (step) ? atoi(step) : 1;
					msg.start_program.params.colorwheel.fade_delay = (delay) ? atoi(delay) : 2;
					msg.start_program.params.colorwheel.fade_sleep = (sleep) ? atoi(sleep) : 0;
					break;

				case 1:
					printf("Start program: random\n");
					msg.start_program.params.random.seed = (uint16_t) (rand() % 0xffff);
					msg.start_program.params.random.use_address = (use_address) ? atoi(use_address) : 1;
					msg.start_program.params.random.wait_for_fade = (wait_for_fade) ? atoi(wait_for_fade) : 1;
					msg.start_program.params.random.min_distance = (min_distance) ? atoi(min_distance) : 60;

					msg.start_program.params.random.saturation = (saturation) ? atoi(saturation) : 255;
					msg.start_program.params.random.value = (value) ? atoi(value) : 255;

					msg.start_program.params.random.fade_step = (step) ? atoi(step) : 25;
					msg.start_program.params.random.fade_delay = (delay) ? atoi(delay) : 3;
					msg.start_program.params.random.fade_sleep = (sleep) ? atoi(sleep) : 10;
					break;

				default:
					return MHD_NO;
			}
		}
		else if (strcmp(url+1, "stop") == 0) {
			msg.cmd = REMOTE_CMD_STOP;
			printf("Stop fading\n");
		}
		else if (strcmp(url+1, "shutdown") == 0) {
			msg.cmd = REMOTE_CMD_POWERDOWN;
			printf("Shutdown fnordlichts\n");
		}
		else {
			return MHD_NO;
		}

		/* send command */
		int p = (mask) ? fn_send_mask(fnordlicht_fd, mask, &msg) : fn_send(fnordlicht_fd, &msg);

		print_cmd(&msg, sizeof(msg));
		printf("Sent %i bytes to fnordlichts\n", p);

		response_str = (p > 0) ? "success" : "failed";
		response = MHD_create_response_from_data(strlen(response_str), (void *) response_str, 1, 1);

		int ret = MHD_queue_response(connection, (p > 0) ? MHD_HTTP_OK : MHD_HTTP_INTERNAL_SERVER_ERROR, response);
		MHD_destroy_response(response);
		return ret;
	}
	else {
		return MHD_NO;
	}
}

int main(int argc, char *argv[]) {
	/* bind signals */
	struct sigaction action;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	action.sa_handler = quit;

	sigaction(SIGINT, &action, NULL);	/* catch ctrl-c from terminal */
	sigaction(SIGHUP, &action, NULL);	/* catch hangup signal */
	sigaction(SIGTERM, &action, NULL);	/* catch kill signal */

	if (argc < 3 || argc > 4) {
		fprintf(stderr, "usage: fnweb SERIAL-PORT WEB-DIRECTORY [HTTPD-PORT]\n");
		return EXIT_FAILURE;
	}

	/* connect to fnordlichts */
	fnordlicht_fd = open(argv[1], O_RDWR | O_NOCTTY);
	if (fnordlicht_fd < 0) {
		fprintf(stderr, "Failed to open fnordlichts: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	struct termios oldtio = fn_init(fnordlicht_fd);
	int p = fn_sync(fnordlicht_fd);
	if (p <= 0) {
		fprintf(stderr, "Failed to initialize fnordlichts!\n");
		exit(EXIT_FAILURE);
	}

	/* seed PNRG for random program */
	srand(time(0));

	/* start embedded HTTPd */
	httpd_port = (argc == 4) ? atoi(argv[3]) : 80; /* default port */
	httpd_root = argv[2];
	printf("Starting HTTPd on port: %i with root dir: %s\n", httpd_port, httpd_root);

	struct MHD_Daemon *httpd = MHD_start_daemon(
		MHD_USE_THREAD_PER_CONNECTION | MHD_USE_DEBUG,
		httpd_port,
		NULL, NULL,
		&handle_request, NULL,
		//MHD_OPTION_CONNECTION_LIMIT, 1,
		MHD_OPTION_END
	);

	if (httpd == NULL) {
		fprintf(stderr, "Failed to start HTTPd\n");
		return EXIT_FAILURE;
	}

	/* busy loop */
	while (!terminate) { }

	/* stop embedded HTTPd */
	MHD_stop_daemon(httpd);

	/* reset and close connection */
	tcsetattr(fnordlicht_fd, TCSANOW, &oldtio);
	close(fnordlicht_fd);

	return EXIT_SUCCESS;
}
