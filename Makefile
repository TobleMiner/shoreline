CC=gcc
CCFLAGS=-O3 -Wall -ggdb
RM=rm -f

all: clean shoreline

shoreline:
	$(CC) $(CCFLAGS) ring.c llist.c framebuffer.c sdl.c network.c main.c -lpthread -lSDL2 `pkg-config --cflags sdl2` -D_GNU_SOURCE -o shoreline

clean:
	$(RM) shoreline
