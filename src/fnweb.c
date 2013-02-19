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
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <microhttpd.h>

#include "libfn.h"

volatile bool terminate = false;/* will be set to TRUE in our signal handler */
char *httpd_root;		/* where static HTML content is located */
int httpd_port;			/* TCP port the webserver should listen to */
int fn_fd;			/* file descriptor for serial port */
int fn_count;
int httpd_users = 0;

pthread_cond_t listen_cond;
pthread_mutex_t listen_mutex;

struct {
	struct rgb_color_t color;
	int step;
	int delay;
} fn_last;

struct rgb_color_t parse_color(const char *identifier) {
	struct rgb_color_t color;
	unsigned short int red, green, blue;

	if (sscanf(identifier, "#%2hX%2hX%2hX", &red, &green, &blue) != 3) {
		fprintf(stderr, "Invalid color definition: %s", identifier);
	}

	color.red = red;
	color.green = green;
	color.blue = blue;

	return color;
}

void quit(int sig) {
	terminate = true;

	/* unblock all waiting listeners */
	pthread_mutex_lock(&listen_mutex);
	pthread_cond_broadcast(&listen_cond);
	pthread_mutex_unlock(&listen_mutex);
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

	if (strcasecmp(method, "get") == 0) {
		struct MHD_Response *response;
		
		if (strcmp(url+1, "status") == 0) {
			char status_str[256];
			char color_str[8];

			/* barrier */
			const char *comet = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "comet");
			if (comet) {
				struct timespec timeout;
				clock_gettime(CLOCK_REALTIME, &timeout);
				timeout.tv_sec += atoi(comet);

				pthread_mutex_lock(&listen_mutex);
				httpd_users++;
				pthread_cond_timedwait(&listen_cond, &listen_mutex, &timeout);
				httpd_users--;
				pthread_mutex_unlock(&listen_mutex);
			}

			snprintf(color_str, 8, "#%02x%02x%02x\n", fn_last.color.red, fn_last.color.green, fn_last.color.blue);
			snprintf(status_str, 255, "{ \"count\": %d, \"users\": %d, \"color\": { \"r\": %d, \"g\": %d, \"b\": %d, \"hex\": \"%s\"}, \"step\": %d, \"delay\": %d }\n",
				fn_count, httpd_users,
				fn_last.color.red, fn_last.color.green, fn_last.color.blue,
				color_str, fn_last.step, fn_last.delay
			);
	
			response = MHD_create_response_from_data(strlen(status_str), (void *) status_str, 0, 1);
			MHD_add_response_header(response, "Content-Type", "application/json");

		}
		else { /* get file from disk */
			char filename[1024] = "";

			/* construct filename */
			strncat(filename, httpd_root, 1024);
			strncat(filename, (strlen(url) > 1) ? url : "/index.html", 1024);

			/* try to open file */
			char *response_str;
			int response_size = load_file(filename, &response_str);
			if (response_size > 0) {
				response = MHD_create_response_from_data(response_size, (void *) response_str, 1, 0);
				MHD_add_response_header(response, "Content-Type", get_content_type(filename));
			}
			else {
				const char *error_str = "<html><body><h2>File not found!</h2></body></html>";
				fprintf(stderr, "Failed to open file: %s\n", filename);
				response = MHD_create_response_from_data(strlen(error_str), (void *) error_str, 0, 0);
				MHD_add_response_header(response, "Content-Type", "text/html");
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

			/* at least the color is required */
			if (color == NULL) {
				return MHD_NO;
			}

			if (color) fn_last.color = parse_color(color); /* parse and updating current color */
			if (step) fn_last.step = atoi(step);
			if (delay) fn_last.delay = atoi(delay);

			msg.cmd = REMOTE_CMD_FADE_RGB;
			msg.fade_rgb.step = (step) ? fn_last.step : 15;
			msg.fade_rgb.delay = (delay) ? fn_last.delay : 5;
			msg.fade_rgb.color = fn_last.color;

			printf("Fading to color: %s\n", color);

			/* unblock all waiting listeners */
			pthread_mutex_lock(&listen_mutex);
			pthread_cond_broadcast(&listen_cond);
			pthread_mutex_unlock(&listen_mutex);
		}
		else if (strcmp(url+1, "start") == 0) {
			/* parameters */
			const char *script = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "script");

			/* at least the color is required */
			if (script == NULL) {
				return MHD_NO;
			}

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
					msg.start_program.params.colorwheel.hue_step = 360 / fn_count;
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
			msg.msg_stop.fade = 1;
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
		int p = (mask) ? fn_send_mask(fn_fd, mask, &msg) : fn_send(fn_fd, &msg);

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

	if (argc < 3 || argc > 5) {
		fprintf(stderr, "usage: fnweb SERIAL-PORT WEB-DIRECTORY [HTTPD-PORT [FNORDLICHT-COUNT]]\n");
		return EXIT_FAILURE;
	}

	/* connect to fnordlichts */
	fn_fd = open(argv[1], O_RDWR | O_NOCTTY);
	if (fn_fd < 0) {
		fprintf(stderr, "Failed to open fnordlichts: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	struct termios oldtio = fn_init(fn_fd);

	if (argc >= 5) {
		fn_count = atoi(argv[4]);
	}
	else {
		fn_count = fn_count_devices(fn_fd);
	}

	/* set startup state */
	fn_last.color.red = 0;
	fn_last.color.green = 0;
	fn_last.color.blue = 0;
	fn_last.step = 255;
	fn_last.delay = 0;

	/* seed PNRG for random program */
	srand(time(0));

	/* initialize listen condition */
	pthread_cond_init(&listen_cond, NULL);
	pthread_mutex_init(&listen_mutex, NULL);

	/* start embedded HTTPd */
	httpd_port = (argc >= 4) ? atoi(argv[3]) : 80; /* default port */
	httpd_root = realpath(argv[2], NULL);

	if (httpd_port == 0) {
		fprintf(stderr, "Invalid HTTPd port: %s\n", argv[3]);
		return EXIT_FAILURE;
	}

	if (httpd_root == NULL) {
		fprintf(stderr, "Invalid HTTPd root directory: %s\n", argv[2]);
		return EXIT_FAILURE;
	}

	printf("Starting HTTPd on port: %i with root dir: %s\n", httpd_port, httpd_root);
	struct MHD_Daemon *httpd = MHD_start_daemon(
		MHD_USE_THREAD_PER_CONNECTION | MHD_USE_POLL,
		httpd_port,
		NULL, NULL,
		&handle_request, NULL,
		MHD_OPTION_CONNECTION_LIMIT, 10,
		MHD_OPTION_END
	);

	if (httpd == NULL) {
		fprintf(stderr, "Failed to start HTTPd\n");
		return EXIT_FAILURE;
	}

	/* busy loop */
	int c = 0;
	while (!terminate) {
		if (c++ % 10 == 0) {
			int p = fn_sync(fn_fd);
			if (p <= 0) {
				fprintf(stderr, "Failed to sync fnordlichts!\n");
				return EXIT_FAILURE;
			}
			else {
				printf("Fnordlicht's resynced, %d users\n", httpd_users);
			}
		}
		sleep(1);
	}

	/* stop embedded HTTPd */
	MHD_stop_daemon(httpd);

	/* reset and close connection */
	tcsetattr(fn_fd, TCSANOW, &oldtio);
	close(fn_fd);

	free(httpd_root);

	return EXIT_SUCCESS;
}
