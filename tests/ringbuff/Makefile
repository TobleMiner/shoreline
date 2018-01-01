CC=gcc
CCFLAGS=-O3 -Wall
RM=rm -f

all: clean test

test:
	$(CC) $(CCFLAGS) main.c -lpthread -o sturmflut

clean:
	$(RM) sturmflut
