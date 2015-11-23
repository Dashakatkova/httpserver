#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

#define BUF_SIZE 100

void headers(int client, int size, int httpcode) {
	char buf[1024];
	char strsize[20];
	sprintf(strsize, "%d", size);
	if (httpcode == 200) {
		strcpy(buf, "HTTP/1.0 200 OK\r\n");
	}
	else if (httpcode == 404) {
		strcpy(buf, "HTTP/1.0 404 Not Found\r\n");
	}
	else {
		strcpy(buf, "HTTP/1.0 500 Internal Server Error\r\n");
	}
	send(client, buf, strlen(buf), 0);
	strcpy(buf, "Connection: keep-alive\r\n");
	send(client, buf, strlen(buf), 0);
        sprintf(buf, "Content-length: %s\r\n", strsize);
	send(client, buf, strlen(buf), 0);
	strcpy(buf, strsize);
	strcpy(buf, "Server: simple-server\r\n");
	send(client, buf, strlen(buf), 0);
	// if image is requested then content type is different
	sprintf(buf, "Content-Type: text/html; charset=UTF-8\r\n");
	send(client, buf, strlen(buf), 0);
	strcpy(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
}


void parseFileName(char *line, char **filepath, size_t *len) {
	char *start = NULL;
	while ((*line) != '/') line++;
	start = line + 1;
	while ((*line) != ' ') line++;
	(*len) = line - start;
	*filepath = (char*)malloc(*len + 1);
	*filepath = strncpy(*filepath, start, *len);
	(*filepath)[*len] = '\0';
	printf("%s \n", *filepath);
}

int main() {
	int ld = 0;
	int res = 0;
	int cd = 0;
	int filesize = 0;
	const int backlog = 10;
	struct sockaddr_in saddr;
	struct sockaddr_in caddr;
	char *line = NULL;
        char *buf = NULL;
	size_t len = 0;
	char *filepath = NULL;
	size_t filepath_len = 0;
	int empty_str_count = 0;
	socklen_t size_saddr;
	socklen_t size_caddr;
	FILE *fd;
	FILE *file;

	// listen to request
	ld = socket(AF_INET, SOCK_STREAM, 0);
	if (ld == -1) {
		printf("listener create error \n");
	}
	// configure listener socket
	// set network parameters
	// socket to be internet socket
	saddr.sin_family = AF_INET;
	// set port
	saddr.sin_port = htons(8080);
	// set to listen in all addresses
	saddr.sin_addr.s_addr = INADDR_ANY;
	// configure
	res = bind(ld, (struct sockaddr *)&saddr, sizeof(saddr));
	if (res == -1) {
		printf("bind error \n");
	}
	// listen to request
	// backlog - queue size
	res = listen(ld, backlog);
	if (res == -1) {
		printf("listen error \n");
	}
	while (1) {
		// cd - client descriptor (unique number for client to respond)
		// connect()
		cd = accept(ld, (struct sockaddr *)&caddr, &size_caddr);
		if (cd == -1) {
			printf("accept error \n");
		}
		printf("client in %d descriptor. Client addr is %d \n", cd, caddr.sin_addr.s_addr);
		fd = fdopen(cd, "r");
		if (fd == NULL) {
			printf("error open client descriptor as file \n");
		}
		while ((res = getline(&line, &len, fd)) != -1) {
			if (strstr(line, "GET")) {
				parseFileName(line, &filepath, &filepath_len);
				// add check for .jpg
			}
			// check for empty string in the end of request
			if (strcmp(line, "\r\n") == 0) {
				empty_str_count++;
			}
			else {
				empty_str_count = 0;
			}
			if (empty_str_count == 1) {
				break;
			}
			printf("%s", line);
		}
		printf("open %s \n", filepath);
		// "b" binary flag - for non text files
		file = fopen(filepath, "rb");
		if (file == NULL) {
			printf("404 File Not Found \n");
			headers(cd, 0, 404);
		}
		else {
			fseek(file, 0L, SEEK_END);
			filesize = ftell(file);
			fseek(file, 0L, SEEK_SET);
			headers(cd, filesize, 200);
                        while (!feof(file)) {
                          len = fread(line, 1, 1024, file);
                          res = send(cd, line, len, 0);
                          if (res == -1) {
                            printf("send error\n");
                          }
                        }
		}
		close(cd);
	}
	return 0;
}