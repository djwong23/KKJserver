CC=gcc
CFLAGS=-Wall -fsanitize=undefined,address -g
OUTPUTS=KKJserver
.PHONY: all clean

all:main

main:Asst3.c
	$(CC) $(CFLAGS) -o KKJserver Asst3.c

clean:
	rm $(OUTPUTS) 
