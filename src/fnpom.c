#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <json/json.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "../lib/libfn.h"

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

char * http_get(char * response, size_t bytes, char * host, char * port, char * path) {
	int sd, count;
	char request[1024];
	struct addrinfo hints, *res;

	/* set up connection */
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;    /* both IPv4 & IPv6 */
	hints.ai_socktype = SOCK_STREAM;

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
	do { count = recv(sd, response, bytes, 0); } while (count > 0);
	/* close socket */
	close(sd);
	
	return response;
}

int main(int argc, char * argv[]) {
	char host[] = "volkszaehler.org";
	char port[] = "80";
	char path[] = "/demo/backend.php/data/";
	char uuid[] = "12345678-1234-1234-1234-123456789012";
	char response[8192], url[1024],  * json;

	struct json_tokener * json_tok;
	struct json_object * json_obj;
	
	sprintf(url, "%s/%s.txt?from=yesterday&tuples=1", path, uuid); /* store request uri in buffer */
	http_get(response, sizeof(response), host, port, url); /* fetch json data to buffer */
	json = http_get_body(response); /* get pointer to http body */
	
	json_tok = json_tokener_new();
	json_obj = json_tokener_parse_ex(json_tok, json, strlen(json));
	
	if (json_tok->err != json_tokener_success) {
		json_obj = error_ptr(-json_tok->err);
		exit(-1);
	}
		
	/* printf("%s\n", json_object_to_json_string(json_obj)); */
	
	json_obj = json_object_object_get(json_obj, "data");
	json_obj = json_object_object_get(json_obj, "last");
	
	if (json_object_get_type(json_obj) != json_type_double) {
		fprintf(stderr, "invalid json!\n");
		exit(-1);
	}
	
	printf("Last value: %.2f\n", json_object_get_double(json_obj));
	
	/* housekeeping */
	json_tokener_free(json_tok);
	
	return 0;
}
