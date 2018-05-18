CC=gcc
CCFLAGS=-Ofast -Wall -march=native
CONFIG=-DUNSAFE_PIXEL_COUNTERS
RM=rm -f

all: clean shoreline

shoreline:
	$(CC) $(CCFLAGS) ring.c llist.c framebuffer.c sdl.c network.c main.c -lpthread `pkg-config --libs --cflags sdl2` -D_GNU_SOURCE $(CONFIG) -o shoreline

clean:
	$(RM) shoreline
