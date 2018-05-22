CC=gcc
CCFLAGS=-Ofast -Wall -march=native
RM=rm -f
DEPS=sdl2 libvncserver
DEPFLAGS:=`pkg-config --libs --cflags $(DEPS)`

all: clean shoreline

shoreline:
	$(CC) $(CCFLAGS) ring.c llist.c framebuffer.c sdl.c vnc.c network.c main.c -lpthread $(DEPFLAGS) -D_GNU_SOURCE -o shoreline

clean:
	$(RM) shoreline
