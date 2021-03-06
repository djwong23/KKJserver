#include <ctype.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#define BACKLOG 5

char *checkMessage(int stage, char *message, char *setUpLine, char *punchLine);
char *findADSError(char *message);
char *findError(int stage, char *message, char *expected, int expectedLen);
int numDigits(int len);
char *lengthAsString(int len);
int readXBytes(int socketFileDesc, char *buff, int x);
struct jokeLines {
	char *setUpLine;
	char *punchLine;
};
typedef struct jokeLines joke;
int main(int argc, char **argv) {
	if (argc != 3) {
		printf("Usage: %s [port] [filepath]\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	FILE *jokeFile = fopen(argv[2], "r");
	if (jokeFile == NULL) {
		printf("Error opening file %s, closing\n", argv[2]);
	}
	srand(time(0));
	int maxJokes = 10;
	joke *jokes = malloc(sizeof(joke) * maxJokes);
	int currJoke = 0;
	char *readBuffer = malloc(500);
	int reachedEOF = 0;
	while (reachedEOF != 1) {
		for (int i = 0; i < 3; i++) {
			bzero(readBuffer, 500);
			if (fgets(readBuffer, 500, jokeFile) == NULL) {
				reachedEOF = 1;
				break;
			}
			if (currJoke == maxJokes) {
				maxJokes *= 2;
				jokes = realloc(jokes, maxJokes);
			}
			if (i == 0) {
				jokes[currJoke].setUpLine = malloc((strlen(readBuffer)) * sizeof(char));
				readBuffer[strlen(readBuffer) - 1] = '\0';
				strcpy(jokes[currJoke].setUpLine, readBuffer);
			} else if (i == 1) {
				jokes[currJoke].punchLine = malloc((strlen(readBuffer)) * sizeof(char));
				readBuffer[strlen(readBuffer) - 1] = '\0';
				strcpy(jokes[currJoke].punchLine, readBuffer);
			} else if (i == 2) {
				continue;
			}
		}
		currJoke++;
	}
	int lastJoke = currJoke;
	currJoke = rand() % (lastJoke);


	//socket setup; socket contained in socketFileDesc
	char *port = argv[1];
	struct addrinfo hint, *address_list, *addr;
	int err, socketFileDesc, connectionFileDesc;
	socketFileDesc = 0;
	socklen_t addrlen = 0;
	struct sockaddr *socketAddress = NULL;
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
					char message[500];
					strcpy(message, "REG|");
					sprintf(&message[4], "%lu|", strlen(jokes[currJoke].setUpLine));
					strcat(message, jokes[currJoke].setUpLine);
					strcat(message, "|");
					write(connectionFileDesc, message, strlen(message));
					break;
				}
				case 4: {
					char message[500];
					strcpy(message, "REG|");
					sprintf(&message[4], "%lu|", strlen(jokes[currJoke].punchLine));
					strcat(message, jokes[currJoke].punchLine);
					strcat(message, "|");
					write(connectionFileDesc, message, strlen(message));
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
				//need to read in all digits of the length character by character, store it in length
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
					if (!isdigit(buff[4 + k]) && buff[4 + k] != '|') {
						buff[4 + k + 1] = '\0';
						// char error[4];
						// error[0] = 'M';
						// error[1] = i + '1';
						// error[2] = 'F';
						// error[3] = 'T';


						char *err = malloc(10 * sizeof(char));
						err[0] = '\0';
						strcat(err, "ERR|");
						err[4] = 'M';
						err[5] = i + '1';
						err[6] = '\0';
						strcat(err, "FT|");
						write(connectionFileDesc, err, 10);
						finished = 1;
						break;
					}
					if (buff[4 + k] == '|') {
						if (k == 0) {
							// char error[4];
							// error[0] = 'M';
							// error[1] = i + '1';
							// error[2] = 'F';
							// error[3] = 'T';
							// write(connectionFileDesc, error, 4);

							char *err = malloc(10 * sizeof(char));
							err[0] = '\0';
							strcat(err, "ERR|");
							err[4] = 'M';
							err[5] = i + '1';
							err[6] = '\0';
							strcat(err, "FT|");
							write(connectionFileDesc, err, 10);
							free(err);
							finished = 1;
							break;
						}
						char *length = malloc((k + 1) * sizeof(char));
						if (length == NULL) {
							printf("Malloc error.\n");
							return 1;
						}
						memcpy(length, &buff[4], k);
						length[k] = '\0';
						int intLength;
						intLength = (int) strtol(length, NULL, 10);
						if (4 + k + intLength >= size) {
							size *= 2;
							buff = realloc(buff, size);
							if (buff == NULL) {
								printf("Malloc error.\n");
								return 1;
							}
						}
						//now we have the length and should read exactly the length given
						int ret = readXBytes(connectionFileDesc, &buff[4 + k + 1], intLength);
						if (ret == 1) {
							printf("Early termination.\n");
							finished = 1;
							break;
						} else if (ret == 2) {
							// char error[4];
							// error[0] = 'M';
							// error[1] = i + '1';
							// error[2] = 'L';
							// error[3] = 'N';

							char *err = malloc(10 * sizeof(char));
							err[0] = '\0';
							strcat(err, "ERR|");
							err[4] = 'M';
							err[5] = i + '1';
							err[6] = '\0';
							strcat(err, "LN|");
							write(connectionFileDesc, err, 10);
							finished = 1;
							free(err);
							break;
						}
						//must read 1 more char for the pipe
						if (read(connectionFileDesc, &buff[4 + k + 1 + intLength], 1) == 0) {
							printf("Early termination.\n");
							finished = 1;
							break;
						}
						buff[4 + k + 1 + intLength + 2] = '\0';
						free(length);
						break;
					}
					k += 1;
				}
			} else if (strcmp("ERR|", buff) == 0) {
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
					char twoChars[3];
					twoChars[2] = '\0';
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
				// char error[10];
				// error[0] = 'E';
				// error[1] = 'R';
				// error[2] = 'R';
				// error[3] = '|';
				// error[4] = 'M';
				// error[5] = i + '1';
				// error[6] = 'F';
				// error[7] = 'T';
				// error[8] = '|';

				char *err = malloc(10 * sizeof(char));
				err[0] = '\0';
				strcat(err, "ERR|");
				err[4] = 'M';
				err[5] = i + '1';
				err[6] = '\0';
				strcat(err, "FT|");
				write(connectionFileDesc, err, 10);
				free(err);
				break;
			}

			if (!finished) {
				//printf("Input %s\n", buff);
				char *e = checkMessage(i + 1, buff, jokes[currJoke].setUpLine, jokes[currJoke].punchLine);//if message if readable so far, it compares message to expectation
				if (e != NULL) {
					write(connectionFileDesc, e, 10);
					break;
				}
			}
		}

		close(connectionFileDesc);
		free(buff);
		currJoke = rand() % (lastJoke);
	}
}

int readXBytes(int socketFileDesc, char *buff, int x) {
	int remaining = x;
	while (remaining > 0) {
		char c = '\0';
		int nRead = read(socketFileDesc, &c, 1);
		if (nRead == 0) {
			return 1;
		}
		if (c == '|') {
			//printf("Pipe hit.\n");
			return 2;
		}
		buff[x - remaining] = c;
		remaining -= 1;
	}
	return 0;
}

//accepts stage of the joke and the input from client
//returns NULL on success, error message on failure
char *checkMessage(int stage, char *message, char *setUpLine, char *punchLine) {
	char *expected;
	char *temp;
	int len;

	//only need to check punctuation on ads
	if (stage == 5) return findADSError(message);

	//first create expected string
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
			expected = malloc((3 + 4 + numDigits(len) + len) * sizeof(char));
			strcat(expected, "REG|");
			strcat(expected, lengthAsString(len));
			strcat(expected, "|");
			strcat(expected, setUpLine);
			strcat(expected, "|");
			break;
		case 3:
			len = strlen(setUpLine) + 6 - 1;
			temp = malloc((strlen(setUpLine) + 1) * sizeof(char));
			strcpy(temp, setUpLine);
			temp[strlen(setUpLine) - 1] = '\0';
			expected = malloc((3 + 4 + numDigits(len) + len) * sizeof(char));
			expected[0] = '\0';
			strcat(expected, "REG|");
			strcat(expected, lengthAsString(len));
			strcat(expected, "|");
			strcat(expected, temp);
			strcat(expected, ", who?");
			strcat(expected, "|\0");
			free(temp);
			break;
		case 4:
			len = strlen(punchLine);
			expected = malloc((3 + 4 + numDigits(len) + len) * sizeof(char));
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

char *findADSError(char *message) {
	int len = strlen(message);
	if (len < 4) return "ERR|M5FT|";
	if (message[0] != 'R' || message[1] != 'E' || message[2] != 'G' || message[3] != '|') return "ERR|MSFT|";

	if (message[len - 1] != '|') return "ERR|M5FT|";

	if (message[len - 2] != '.' && message[len - 2] != '?' && message[len - 2] != '!') return "ERR|M5CT|";

	return NULL;
}

char *findError(int stage, char *message, char *expected, int expectedLen) {
	char *err = malloc(10 * sizeof(char));
	err[0] = '\0';
	strcat(err, "ERR|");
	err[4] = 'M';
	err[5] = stage + 48;
	err[6] = '\0';


	if (message[0] != expected[0] || message[1] != expected[1] || message[2] != expected[2] || message[3] != '|')
		return strcat(err, "FT|");

	int mlen = strlen(message);
	if (message[mlen - 1] != '|') return strcat(err, "FT|");

	int lenJoke = 0;

	int i = 4;
	int messageLen = strlen(message);
	while (i < messageLen) {
		if (message[i] >= 48 && message[i] <= 57) {
			lenJoke = lenJoke * 10 + (message[i] - 48);
		} else if (message[i] == '|') {
			if (lenJoke == 0)
				return strcat(err, "FT|");
			else
				break;
		} else {
			return strcat(err, "FT|");
		}
		i++;
	}

	if (lenJoke != expectedLen) {
		return strcat(err, "LN|");
	}

	int start = i + 1;

	for (i = start; i < start + lenJoke; i++) {
		if (message[i] == '|') return strcat(err, "FT|");
		if (message[i] != expected[i]) return strcat(err, "CT|");
	}

	if (message[i] != '|') return strcat(err, "FT|");

	return strcat(err, "FT|");
}

char *lengthAsString(int len) {
	int digits = numDigits(len);
	char *res = malloc((digits + 1) * sizeof(char));
	sprintf(res, "%d", len);
	res[digits] = '\0';
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
