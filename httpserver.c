#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

#define N 5

#include <sys/queue.h>

struct {
	char *ext;
	char *conttype;
} extensions[] = {
	{".txt", "text/html"},
	{".htm", "text/html"},
	{".html", "text/html"},
	{".jpg", "text/jpeg"},
	{".jpeg", "text/jpg"},
	{".png", "image/png"},
	{".ico", "image/ico"},
	{".css", "text/css"},
	{".js", "text/javascript"},
	{".php", "text/php"},
	{".xml", "text/xml"},
	{".pdf", "application/pdf"},
	{0, 0}
};

pthread_t ntid[N];
pthread_t servtid;
pthread_mutex_t lock[N];
int cd[N];

struct qnode {
	int value;
	TAILQ_ENTRY(qnode) entries;
};
TAILQ_HEAD(, qnode) qhead;

void headers(int client, int size, int httpcode, char* content_type) {
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
	if (content_type != NULL) {
		sprintf(buf, "Content-Type: %s\r\n", content_type);
		send(client, buf, strlen(buf), 0);
		strcpy(buf, "\r\n");
		send(client, buf, strlen(buf), 0);
	}
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

void *handler(void *arg) {
	int filesize = 0;
	char *line = NULL;
	size_t len = 0;
	char buf[1024];
	char *filepath = NULL;
	size_t filepath_len = 0;
	int empty_str_count = 0;
	FILE *fd;
	FILE *file;

	// getting handler number
	int *p = (int *) arg;
	int k = *p;

	// wait for lock
	pthread_mutex_lock(&lock[k]);
	pthread_mutex_unlock(&lock[k]);

	//try opening client descriptor
	fd = fdopen(cd[k], "r");
	if (fd == NULL) {
		printf("error open client descriptor as file \n");
		printf("500 Internal Server Error \n");
		headers(cd[k], 0, 500, NULL);
	} else {
		// reading client's headers
		int res;
		while ((res = getline(&line, &len, fd)) != -1) {
			if (strstr(line, "GET")) {
				parseFileName(line, &filepath, &filepath_len);
			}
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
			file = fopen(filepath, "rb");
			if (file == NULL) {
				printf("404 File Not Found \n");
				headers(cd[k], 0, 404, NULL);
			}
			else {
				char *fileext = strrchr(filepath, '.');
				char *content_type = 0;
				int i = 0;
				while (extensions[i].ext != 0) {
					if (strcmp(extensions[i].ext, fileext) == 0) {
						int n = strlen(extensions[i].conttype);
						content_type = (char*) malloc(n * sizeof(char));
						strncpy(content_type, extensions[i].conttype, n);
						break;
					}
					i++;
				}
				if (content_type != 0) {
					fseek(file, 0L, SEEK_END);
					filesize = ftell(file);
					fseek(file, 0L, SEEK_SET);
					headers(cd[k], filesize, 200, content_type);
					size_t nbytes = 0;
					while ((nbytes = fread(buf, 1, 1024, file)) > 0) {
						res = send(cd[k], buf, nbytes, 0);
						if (res == -1) {
							printf("send error \n");
						}
					}
					free(content_type);
				}
				else {
					printf("500 Internal Server Error \n");
					headers(cd[k], 0, 500, NULL);
				}
		}
	}
	close(cd[k]);
	free(p);
	cd[k] = -1;
	puts ("Handler destroyed");
}

void createThread(int k) {
	int *m = (int *)malloc(sizeof(int));
	*m = k;
	int err = pthread_create(&ntid[k], NULL, handler, (void *) m);
	if (err != 0) {
		printf("it's impossible to create a thread %s\n", strerror(err));
	}
}

void *serv(void *arg) {
	int i;
	while (1) {
		i = 0;
		while (i < N) {
			if (pthread_mutex_trylock(&lock[i]) == 0) {
				// mutex is acquired -> thread is working or dead
				if (cd[i] == -1) { // thread is dead
					createThread(i);
					printf("Handler %d recreated\n", i);
				}
				else pthread_mutex_unlock(&lock[i]);
			} else {
				// mutex is not acquired -> thread is ready
				struct qnode *item = TAILQ_FIRST(&qhead);
				if (item != NULL) {
					puts("Handler found");
					cd[i] = item->value;
					pthread_mutex_unlock(&lock[i]); // unlock mutex
					// delete item from queue
					TAILQ_REMOVE(&qhead, item, entries);
					free(item);
					break;
				}
			}
			i++;
		}
	}
}

int main() {
	int ld = 0;
	int res = 0;
	int _cd = 0;
	const int backlog = 10;
	struct sockaddr_in saddr;
	struct sockaddr_in caddr;
	socklen_t size_saddr;
	socklen_t size_caddr;
	struct qnode *qitem;
	int i = 0;

	//init queue
	TAILQ_INIT(&qhead);

	// creating threads
	while (i < N) {
		pthread_mutex_init(&lock[i], NULL);
		pthread_mutex_lock(&lock[i]);
		createThread(i);
		i++;
	}

	int err = pthread_create(&servtid, NULL, serv, NULL);
	if (err != 0) {
		printf("it's impossible to create a thread %s\n", strerror(err));
	}

	ld = socket(AF_INET, SOCK_STREAM, 0);
	if (ld == -1) {
		printf("listener create error \n");
	}

	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(8080);
	saddr.sin_addr.s_addr = INADDR_ANY;
	if (bind(ld, (struct sockaddr *)&saddr, sizeof(saddr)) == -1) {
		printf("bind error \n");
	}
	res = listen(ld, backlog);
	if (res == -1) {
		printf("listen error \n");
	}

	puts("Start");
	while (1) {
		_cd = accept(ld, (struct sockaddr *)&caddr, &size_caddr);
		if (_cd == -1) {
			printf("accept error \n");
		}

		printf("client in %d descriptor. Client addr is %d \n", _cd, caddr.sin_addr.s_addr);

		qitem = malloc(sizeof(*qitem));
		qitem->value = _cd;
		TAILQ_INSERT_TAIL(&qhead, qitem, entries);
	}
	return 0;
}