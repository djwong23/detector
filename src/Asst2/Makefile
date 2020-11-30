CC=gcc
CFLAGS=-pthread -Wall
OUTPUTS=detector
.PHONY: all clean

all:main

main:Asst2.c
	$(CC) $(CFLAGS) -o detector Asst2.c -lm

clean:
	rm $(OUTPUTS) 
