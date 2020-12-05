CC=gcc
CFLAGS=-Wall
OUTPUTS=KKJserver
.PHONY: all clean

all:main

main:Asst3.c
	$(CC) $(CFLAGS) -o KKJserver Asst3.c

clean:
	rm $(OUTPUTS) 
