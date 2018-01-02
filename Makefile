CC=gcc
CCFLAGS=-O3 -Wall
RM=rm -f

all: clean shoreline

shoreline:
	$(CC) $(CCFLAGS) ring.c framebuffer.c network.c main.c -lpthread -o shoreline

clean:
	$(RM) shoreline
