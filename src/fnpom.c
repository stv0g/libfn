#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <json/json.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "../lib/libfn.h"

char * http_get_body(char * buffer) {
	char * body;

	for (body = buffer; body != '\0'; body++) {
		if (body - buffer > 4) {
			if (body[0] == '\r' && body[1] == '\n' && body[2] == '\r' && body[3] == '\n') {
				return body + 4;
			}
		
		}
	}
	
	return NULL;
}

int main(int argc, char * argv[]) {
	int sd, count;
	
	char host[] = "volkszaehler.org";
	char port[] = "80";
	char path[] = "/demo/backend.php/data/";
	char uuid[] = "12345678-1234-1234-1234-123456789012";
	char buffer[8192];
	
	struct addrinfo hints, *res;
	struct json_tokener *tok;
	struct json_object *obj;

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
	sprintf(buffer, "GET %s/%s.json?from=yesterday&tuples=1 HTTP/1.0\r\n\r\n", path, uuid);
	send(sd, buffer, strlen(buffer), 0);

	/* receive data */
	do { count = recv(sd, buffer, sizeof(buffer), 0); } while (count > 0);

	char * json = http_get_body(buffer);
	printf("%s\n", json);
	
	tok = json_tokener_new();
	obj = json_tokener_parse_ex(tok, json, strlen(json));
	
	if (tok->err != json_tokener_success)
		obj = error_ptr(-tok->err);
		
	printf("\n%s\n", json_object_to_json_string(obj));
	
	/* housekeeping */
	json_tokener_free(tok);
	close(sd);
	
	return 0;
}
