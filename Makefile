CC ?= gcc
CCFLAGS = -Ofast -Wall -march=native -D_GNU_SOURCE
RM = rm -f
DEPS = sdl2 libvncserver
DEPFLAGS_CC = `pkg-config --cflags $(DEPS)`
DEPFLAGS_LD = `pkg-config --libs $(DEPS)` -lpthread
OBJS = ring.o llist.o framebuffer.o sdl.o vnc.o network.o main.o

all: shoreline

%.o : %.c
	$(CC) -c $(CCFLAGS) $(DEPFLAGS_CC) $< -o $@

shoreline: $(OBJS)
	$(CC) $(CCFLAGS) $^ $(DEPFLAGS_LD) -o shoreline

clean:
	$(RM) $(OBJS)
	$(RM) shoreline
