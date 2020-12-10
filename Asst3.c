#include <ctype.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BACKLOG 5

char *checkMessage(int stage, char *message, char *setUpLine, char *punchLine);
char *findADSError(char *message);
char *findError(int stage, char *message, char *expected, int expectedLen);
int numDigits(int len);
char *lengthAsString(int len);
int readXBytes(int socketFileDesc, char *buff, int x);
int main(int argc, char **argv) {
	if (argc != 2) {
		printf("Usage: %s [port]\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	//socket setup; socket contained in socketFileDesc
	char *port = argv[1];
	struct addrinfo hint, *address_list, *addr;
	int err, socketFileDesc, connectionFileDesc;
	socketFileDesc = 0;
	socklen_t addrlen = 0;
	struct sockaddr *socketAddress = NULL;
	char setUpLine[] = "Boo.";
	char setUpLineFormat[] = "REG|4|Boo.|";
	char punchLine[] = "Do not cry.";
	char punchLineFormat[] = "REG|11|Do not cry.|";
	memset(&hint, 0, sizeof(struct addrinfo));
	hint.ai_family = AF_UNSPEC;
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_flags = AI_PASSIVE;
	err = getaddrinfo(NULL, port, &hint, &address_list);
	if (err != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
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
	//accepts connection, opens new socket in connectionFileDesc to allow communication
	for (;;) {

		connectionFileDesc = accept(socketFileDesc, socketAddress, &addrlen);
		printf("Accepted\n");
		if (connectionFileDesc == -1) {
			perror("accept");
			continue;
		}
		int size = 100;
		char *buff = malloc(sizeof(char) * size);
		if (buff == NULL) {
			printf("Malloc error.\n");
			return 1;
		}
		int finished = 0;
		//reading & checking all stages of the joke
		for (int i = 0; i <= 6 && !finished; i += 2) {
			bzero(buff, size);
			switch (i) {
				case 0: {
					char intro[] = "REG|13|Knock, knock.|";
					write(connectionFileDesc, intro, strlen(intro));
					break;
				}
				case 2: {
					write(connectionFileDesc, setUpLineFormat, strlen(setUpLineFormat));
					break;
				}
				case 4: {
					write(connectionFileDesc, punchLineFormat, strlen(punchLineFormat));
					break;
				}
				case 6: {
					finished = 1;
					continue;
				}
				default: {
					return 1;
				}
			}

			//readXBytes repeatedly reads until the bytes requested are read
			//returns 1 if the client terminates early
			//returns 2 if the message encounters a pipe as it's reading (early ending)
			if (readXBytes(connectionFileDesc, buff, 3) == 1) {
				printf("Early termination of client.\n");
				break;
			}
			read(connectionFileDesc, &buff[3], 1);
			buff[4] = '\0';
			if (strcmp("REG|", buff) == 0) {
				int k = 0;
				//need to read in all digits of the length by character, store it in length
				//then convert length to a usable int
				while (1) {
					if (4 + k == size) {
						size *= 2;
						buff = realloc(buff, size);
						if (buff == NULL) {
							printf("Malloc error.\n");
							return 1;
						}
					}
					if (read(connectionFileDesc, &buff[4 + k], 1) == 0) {
						printf("Early termination of client. \n");
						finished = 1;
						break;
					}
					printf("Read in %c\n", buff[4 + k]);
					if (!isdigit(buff[4 + k]) && buff[4 + k] != '|') {
						buff[4 + k + 1] = '\0';
						printf("%s is the buff\n", buff);
						printf("Message format broken; %c is not a digit.\n", buff[4 + k]);
						char error[4];
						error[0] = 'M';
						error[1] = i + '1';
						error[2] = 'F';
						error[3] = 'T';
						write(connectionFileDesc, error, 4);
						finished = 1;
						break;
					}
					if (buff[4 + k] == '|') {
						if (k == 0) {
							printf("Message format broken; no length specified.\n");
							char error[4];
							error[0] = 'M';
							error[1] = i + '1';
							error[2] = 'F';
							error[3] = 'T';
							write(connectionFileDesc, error, 4);
							finished = 1;
							break;
						}
						char *length = malloc((k + 1 + 1) * sizeof(char));
						if (length == NULL) {
							printf("Malloc error.\n");
							return 1;
						}
						memcpy(length, &buff[4], k + 1);
						length[k + 2] = '\0';
						int intLength;
						intLength = (int) strtol(length, NULL, 10);
						printf("THE LENGTH IS %d\n", intLength);
						if (4 + k + intLength >= size) {
							size *= 2;
							buff = realloc(buff, size);
							if (buff == NULL) {
								printf("Malloc error.\n");
								return 1;
							}
						}
						int ret = readXBytes(connectionFileDesc, &buff[4 + k + 1], intLength);
						if (ret == 1) {
							printf("Early termination.\n");
							finished = 1;
							break;
						} else if (ret == 2) {
							printf("Message length invalid; pipe hit before specified length.\n");
							char error[4];
							error[0] = 'M';
							error[1] = i + '1';
							error[2] = 'L';
							error[3] = 'N';
							write(connectionFileDesc, error, 4);
							finished = 1;
							break;
						}
						if (read(connectionFileDesc, &buff[4 + k + 1 + intLength], 1) == 0) {
							printf("Early termination.\n");
							finished = 1;
							break;
						}
						printf("Read in %.*s as a string\n", intLength + 1, &buff[4 + k + 1]);
						buff[4 + k + 1 + intLength + 2] = '\0';
						free(length);
						break;
					}
					k += 1;
				}
			} else if (strcmp("ERR|", buff) == 0) {
				printf("Danger!\n");
				//need to read in all of the error message
				char errorCode[6];
				int ret = readXBytes(connectionFileDesc, errorCode, 4);
				if (ret == 1) {
					printf("Early termination by client.\n");
				} else if (ret == 2) {
					printf("Malformed error code; code too short.\n");
				} else if (read(connectionFileDesc, &errorCode[4], 1) == 0) {
					printf("Early termination of client.\n");
				}
				//code is 4 characters and terminated, should be either unrecognized or valid
				else {
					errorCode[5] = '\0';
					printf("Found code!\n");
					char twoChars[2];
					memcpy(twoChars, &errorCode[2], 2 * (sizeof(char)));
					if (errorCode[4] != '|') {
						printf("Malformed error code; no pipe terminator. \n");
					} else if (errorCode[0] != 'M' || errorCode[1] != (i + '0') || (strcmp(twoChars, "CT") != 0 && strcmp(twoChars, "LN") != 0 && strcmp(twoChars, "FT"))) {
						printf("Unrecognized error code.\n");
					} else {
						printf("Client sent error message: ERR|");
						for (int j = 0; j < 5; j++) {
							printf("%c", errorCode[j]);
						}
						printf("\n");
					}
				}
				finished = 1;

			} else {
				printf("Message format broken; %s is the buffer.\n", buff);
				char error[4];
				error[0] = 'M';
				error[1] = i + '1';
				error[2] = 'F';
				error[3] = 'T';
				write(connectionFileDesc, error, 4);
				break;
			}

			if (!finished) {
				printf("Input %s\n", buff);
				char *e = checkMessage(i + 1, buff, setUpLine, punchLine);
				if (e != NULL) {
					printf("error found by checkmsg\n");
					printf("stage: %d\n", i+1);
					write(connectionFileDesc, e, 4);
					break;
				}
			}
		}

		close(connectionFileDesc);
		free(buff);
	}
}

int readXBytes(int socketFileDesc, char *buff, int x) {
	int remaining = x;
	while (remaining > 0) {
		char c = '\0';
		int nRead = read(socketFileDesc, &c, 1);
		if (nRead == 0) {
			printf("EOF reached\n");
			return 1;
		}
		if (c == '|') {
			printf("Pipe hit.\n");
			return 2;
		}
		printf("%c read in readXBytes \n", c);
		buff[x - remaining] = c;
		remaining -= 1;
	}
	printf("Returning in readXBytes\n");
	return 0;
}

//accepts stage of the joke and the input from client
//returns "0" on success, error message on failure

char *checkMessage(int stage, char *message, char *setUpLine, char *punchLine) {
	printf("Checking message %s at stage %d\n", message, stage);
	char *expected;
	char *temp;
	int len;

	if(stage == 5) return findADSError(message);

	switch (stage) {
		case 0:
			len = 13;
			expected = "REG|13|Knock, knock.|";
			break;
		case 1:
			len = 12;
			expected = "REG|12|Who's there?|";
			break;
		case 2:
			len = strlen(setUpLine);
			expected = malloc((3 + 3 + numDigits(len) + len) * sizeof(char));
			strcat(expected, "REG|");
			strcat(expected, lengthAsString(len));
			strcat(expected, "|");
			strcat(expected, setUpLine);
			strcat(expected, "|");
			break;
		case 3:
			len = strlen(setUpLine) + 6 - 1;
			temp = malloc(strlen(setUpLine) * sizeof(char));
			strcpy(temp, setUpLine);
			temp[strlen(setUpLine) - 1] = '\0';
			expected = malloc((3 + 3 + numDigits(len) + len) * sizeof(char));
			strcat(expected, "REG|");
			strcat(expected, lengthAsString(len));
			strcat(expected, "|");
			strcat(expected, temp);
			strcat(expected, ", who?");
			strcat(expected, "|\0");
			printf("seting expected to %s\n", expected);
			free(temp);
			break;
		case 4:
			len = strlen(punchLine);
			expected = malloc((3 + 3 + numDigits(len) + len) * sizeof(char));
			strcat(expected, "REG|");
			strcat(expected, lengthAsString(len));
			strcat(expected, "|");
			strcat(expected, punchLine);
			strcat(expected, "|");
			break;
		default:
			break;
	}

	if (strcmp(expected, message) != 0) return findError(stage, message, expected, len);

	return NULL;
}

char *findADSError(char *message){
	int len = strlen(message);
	if(len < 4) return "M5FT\n";
	if(message[0] != 'R' || message[1] != 'E' || message[2] != 'G' || message[3] != '|') return "MSFT\n";

	if(message[len-1] != '|') return "M5FT\n";

	if(message[len-2] != '.' && message[len-2] != '?' && message[len-2] != '!') return "M5CT\n";

	return NULL;
}



char *findError(int stage, char *message, char *expected, int expectedLen) {
	printf("%s %s\n", expected, message);
	printf("%d\n", expectedLen);

	char *err = malloc(5 * sizeof(char));
	err[0] = 'M';
	err[1] = stage + 48;
	err[2] = '\0';


	if (message[0] != expected[0] || message[1] != expected[1] || message[2] != expected[2] || message[3] != '|')
		return strcat(err, "FT");

	int mlen = strlen(message);
	if(message[mlen-1] != '|') return strcat(err, "FT");

	int lenJoke = 0;

	int i = 4;
	int messageLen = strlen(message);
	while (i < messageLen) {
		if (message[i] >= 48 && message[i] <= 57) {
			lenJoke = lenJoke * 10 + (message[i] - 48);
		} else if (message[i] == '|') {
			if (lenJoke == 0)
				return strcat(err, "FT");
			else
				break;
		} else {
			return strcat(err, "FT");
		}
		i++;
	}

	if (lenJoke != expectedLen) {
		return strcat(err, "LN");
	}

	int start = i + 1;

	for (i = start; i < start + lenJoke; i++) {
		if (message[i] == '|') return strcat(err, "FT");
		if (message[i] != expected[i]) return strcat(err, "CT");
	}

	if (message[i] != '|') return strcat(err, "FT");

	return strcat(err, "FT");
}


// char* findError(int stage, char* message, char* expected){

//first check first three characters - must be REG
//then check that there is a |
//then get length - must equal expected
//then check that there is a |
//then read the content string
//if its length is not equal to read length - problem
//if its content not equal to expected - error
//must have


// }


char *lengthAsString(int len) {
	int digits = numDigits(len);
	char *res = malloc(digits * sizeof(char));
	sprintf(res, "%d", len);
	return res;
}

int numDigits(int len) {
	int res = 0;

	while (len > 0) {
		res++;
		len = len / 10;
	}
	return res;
}
