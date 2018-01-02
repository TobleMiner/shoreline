CC=gcc
CCFLAGS=-O3 -Wall
RM=rm -f

all: clean shoreline

shoreline:
	$(CC) $(CCFLAGS) ring.c framebuffer.c sdl.c network.c main.c -lpthread -lSDL2 `pkg-config --cflags sdl2` -o shoreline

clean:
	$(RM) shoreline
