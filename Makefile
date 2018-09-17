CC ?= gcc
CCFLAGS = -Wall -D_GNU_SOURCE
ifeq ($(DEBUG),1)
CCFLAGS += -O1 -ggdb
else
CCFLAGS += -Ofast -march=native
endif
RM = rm -f
DEPS = sdl2 libvncserver
DEPFLAGS_CC = `pkg-config --cflags $(DEPS)`
DEPFLAGS_LD = `pkg-config --libs $(DEPS)` -lpthread -lnuma
OBJS = ring.o llist.o framebuffer.o sdl.o vnc.o network.o main.o workqueue.o frontend.o

all: shoreline

%.o : %.c
	$(CC) -c $(CCFLAGS) $(DEPFLAGS_CC) $< -o $@

shoreline: $(OBJS)
	$(CC) $(CCFLAGS) $^ $(DEPFLAGS_LD) -o shoreline

clean:
	$(RM) $(OBJS)
	$(RM) shoreline
