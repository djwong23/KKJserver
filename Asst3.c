#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#define MAX 80
#define BACKLOG 5


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
			// checkMessage(i, buff);
		}
	}
}
//accepts stage of the joke and the input from client
//returns "0" on success, error message on failure

char *checkMessage(int stage, char *message, char* setUpLine, char* punchLine, char* adsLine);
char* findError(int stage, char* message, char* expected, int expectedLen);
int numDigits(int len);
char* lengthAsString(int len);

char *checkMessage(int stage, char *message, char* setUpLine, char* punchLine, char* adsLine) {
	char* expected; 
  int len;
	switch (stage)
	{
	case 0:
    len = 13;
		expected = "REG|13|Knock, knock|"; 
		break;
	case 1: 
    len= 12;
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
		len = strlen(setUpLine)+6;
    expected = malloc((3 + 3 + numDigits(len) + len) * sizeof(char));
    strcat(expected, "REG|");
    strcat(expected, lengthAsString(len));
    strcat(expected, "|");
    strcat(expected, setUpLine);
    strcat(expected, ", who?");
    strcat(expected, "|");
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
  case 5: 
    len = strlen(adsLine);
    expected = malloc((3 + 3 + numDigits(len) + len) * sizeof(char));
    strcat(expected, "REG|");
    strcat(expected, lengthAsString(len));
    strcat(expected, "|");
    strcat(expected, adsLine);
    strcat(expected, "|");
	default:
		break;
	}

  if(strcmp(expected, message) != 0) return findError(stage, message, expected, len);
	
	return NULL;
}


char* findError(int stage, char* message, char* expected, int expectedLen){
  
  char* err = malloc(5*sizeof(char));
  err[0] = 'M';
  err[1] = stage + 48;
  err[2] = '\0';


  if(message[0] != expected[0] || message[1] != expected[1] || message[2] != expected[2] || message[3] != '|') 
    return strcat(err, "FT");

  int lenJoke = 0; 

  int i = 4;
  int messageLen = strlen(message);
  while( i < messageLen){
    if(message[i] >= 48 && message[i] <= 57){
      lenJoke = lenJoke* 10 + (message[i]-48);
    }
    else if(message[i] == '|'){
      if(lenJoke == 0)
        return strcat(err, "FT");
      else break;
    }
    else{
      return strcat(err, "FT"); 
    }
    i++;
  }

  if(lenJoke != expectedLen) {
    return strcat(err, "LN");
  }

  int start = i+1;

  for(i = start; i < start+lenJoke; i++){
    if(message[i] == '|') return strcat(err, "FT");
    if(message[i] != expected[i]) return strcat(err, "CT");
  }

  if(message[i] != '|') return strcat(err, "FT");

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



char* lengthAsString(int len){
  int digits = numDigits(len);
  char* res = malloc(digits* sizeof(char));
  sprintf(res, "%d", len);
  return res;
}

int numDigits(int len){
  int res = 0; 

  while(len > 0){
    res++;
    len = len/10;
  }
  return res;
}
