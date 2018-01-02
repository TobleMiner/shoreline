CC=gcc
CCFLAGS=-O0 -Wall -ggdb
RM=rm -f

all: clean test

test:
	$(CC) $(CCFLAGS) ../../ring.c main.c -o test

clean:
	$(RM) test
