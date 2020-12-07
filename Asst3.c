#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#define MAX 80
#define BACKLOG 5

char *checkMessage(int stage, char *message);

int main(int argc, char **argv) {
	if (argc != 2) {
		printf("Usage: %s [port]\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	char *port = argv[1];
	struct addrinfo hint, *address_list, *addr;
	int error, socketFileDesc, connectionFileDesc;
	socketFileDesc = 0;
	socklen_t addrlen = 0;
	struct sockaddr *socketAddress = NULL;
	char setUpLine[] = "Boo.\n";
	char punchLine[] = "Do not cry.\n";
	memset(&hint, 0, sizeof(struct addrinfo));
	hint.ai_family = AF_UNSPEC;
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_flags = AI_PASSIVE;
	error = getaddrinfo(NULL, port, &hint, &address_list);
	if (error != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
		return -1;
	}
	for (addr = address_list; addr != NULL; addr = addr->ai_next) {
		socketFileDesc =
				socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (socketFileDesc == -1) {
			continue;
		}
		if (bind(socketFileDesc, addr->ai_addr, addr->ai_addrlen) == 0 &&
			listen(socketFileDesc, BACKLOG) == 0) {
			break;
		}
		close(socketFileDesc);
	}
	if (addr == NULL) {
		fprintf(stderr, "Could not bind\n");
		return -1;
	}
	freeaddrinfo(address_list);
	printf("Waiting for connection\n");
	for (;;) {

		connectionFileDesc = accept(socketFileDesc, socketAddress, &addrlen);
		printf("Accepted\n");
		if (connectionFileDesc == -1) {
			perror("accept");
			continue;
		}
		char buff[MAX];
		//currently reads until CTRL-C given, sends input to checkMessage
		for (int i = 1; i <= 4; i++) {
			bzero(buff, MAX);
			switch (i) {
				case 1: {
					char intro[] = "Knock, knock.\n";
					write(connectionFileDesc, intro, sizeof(intro));
				} break;
				case 2: {
					write(connectionFileDesc, setUpLine, sizeof(setUpLine));
				} break;
				case 3: {
					write(connectionFileDesc, punchLine, sizeof(punchLine));
				} break;
				case 4: {
					return 0;
				}
				default: {
					return 1;
				}
			}

			int nread = read(connectionFileDesc, buff, MAX);
			buff[nread] = '\0';
			if (nread == 0) {
				printf("Got EOF\n");
				return 0;
			}
			checkMessage(i, buff);
		}
	}
}
//accepts stage of the joke and the input from client
//returns "0" on success, error message on failure
char *checkMessage(int stage, char *message) {
	return "0";
}
