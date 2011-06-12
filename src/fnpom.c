#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <json/json.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "libfn.h"

char * http_get_body(char * response) {
	char * body;

	for (body = response; body != '\0'; body++) {
		if (body - response > 4) {
			if (body[0] == '\r' && body[1] == '\n' && body[2] == '\r' && body[3] == '\n') {
				return body + 4;
			}

		}
	}

	return NULL;
}

struct rgb_color_t calc_gradient(double quota, struct rgb_color_t start, struct rgb_color_t end) {
	struct rgb_color_t gradient = start;

	gradient.red = (end.red - start.red) * quota;
	gradient.green = (end.green - start.green) * quota;
	gradient.blue = (end.blue - start.blue) * quota;

	return gradient;
}

char * http_get(char * response, size_t bytes, char * host, char * port, char * path) {
	int sd, count;
	char request[1024];
	struct addrinfo hints, *res;

	/* set up connection */
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;    /* both IPv4 & IPv6 */
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	getaddrinfo(host, port, &hints, &res);

	sd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	connect(sd, res->ai_addr, res->ai_addrlen);
	if (sd < 0) {
		perror(host);
		exit(-1);
	}

	/* send request */
	sprintf(request, "GET %s HTTP/1.0\r\n\r\n", path);
	send(sd, request, strlen(request), 0);

	/* receive data */
	while ( (count = recv(sd, response, bytes, 0)) > 0)
	if (count < 0) {
		perror("Error receiving data");
		exit(-1);
	}
	/* close socket */
	close(sd);

	return response;
}

int main(int argc, char * argv[]) {
	char host[] = "volkszaehler.org";
	char port[] = "80";
	char path[] = "/demo/middleware.php/data";
	char uuid[] = "12345678-1234-1234-1234-123456789012";
	char device[] = "/dev/ttyUSB0";
	char response[8192], url[1024],  * json;

	int debug = 1, fd;

	struct json_tokener * json_tok;
	struct json_object * json_obj;
	struct termios oldtio;
	struct rgb_color_t start = {{{0, 0xff, 0}}};
	struct rgb_color_t end = {{{0xff, 0, 0}}};

	/* build url path */
	sprintf(url, "%s/%s.json?from=last%%20year&tuples=1", path, uuid); /* store request uri in buffer */

	if (debug) printf("url: http://%s:%s%s\n", host, port, url);

	/* get json */
	http_get(response, sizeof(response), host, port, url); /* fetch json data to buffer */
	json = http_get_body(response); /* get pointer to http body */

	/* parse json */
	json_tok = json_tokener_new();
	json_obj = json_tokener_parse_ex(json_tok, json, strlen(json));

	if (json_tok->err != json_tokener_success) {
		fprintf(stderr, "failed to parse json: %s\n", json_tokener_errors[json_tok->err]);
		exit(-1);
	}

	if (debug) printf("%s\n", json_object_to_json_string(json_obj));

	json_obj = json_object_object_get(json_obj, "data");

	double last = json_object_get_double(json_object_object_get(json_obj, "last"));
	double max = json_object_get_double(json_object_object_get(json_object_object_get(json_obj, "max"), "value"));
	double min = json_object_get_double(json_object_object_get(json_object_object_get(json_obj, "min"), "value"));

	/* calc quota and color for lighting */
	double quota = (last - min) / (max - min);

	if (debug) {
		printf("Last value: %.2f\n", last);
		printf("Min value: %.2f\n", min);
		printf("Max value: %.2f\n", max);
		printf("Quota: %d%%\n", (int) (quota*100));
	}

	struct rgb_color_t gradient = calc_gradient(quota, start, end);

	if (debug) printf("resulting color: #%0X%0X%0X\n", gradient.red, gradient.green, gradient.blue);

	/* fade fnordlichts */
	if (debug) printf("connect via rs232: %s\n", device);

	fd = open(device, O_RDWR | O_NOCTTY);
	if (fd < 0) {
		perror(device);
		exit(-1);
	}
	oldtio = fn_init(fd);
	usleep(25000);
	fn_sync(fd);
	usleep(25000);

	struct remote_msg_t fn_cmd;
	fn_cmd.address = 255;
	fn_cmd.cmd = REMOTE_CMD_FADE_RGB;
	
	fn_cmd.fade_rgb.step = 255;
	fn_cmd.fade_rgb.delay = 0;
	fn_cmd.fade_rgb.color = gradient;

	if (fn_send(fd, (struct remote_msg_t *) &fn_cmd) < 0) {
		perror(device);
		exit(-1);
	}

	/* housekeeping */
	json_tokener_free(json_tok); /* free json objects */
	tcsetattr(fd, TCSANOW, &oldtio); /* reset serial port */

	return 0;
}
