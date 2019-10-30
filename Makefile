CC ?= gcc
RM = rm -f

CCFLAGS = -Wall -D_GNU_SOURCE
ifeq ($(DEBUG),1)
	CCFLAGS += -O1 -ggdb
else
	CCFLAGS += -Ofast -march=native
endif

DEPS = sdl2 libvncserver freetype2
DEPFLAGS_CC = `pkg-config --cflags $(DEPS)`
DEPFLAGS_LD = `pkg-config --libs $(DEPS)` -lpthread -lnuma
OBJS = ring.o llist.o framebuffer.o sdl.o vnc.o network.o main.o workqueue.o frontend.o textrender.o
HDRS = $(wildcard *.h)

all: shoreline

%.o : %.c $(HDRS)
	$(CC) -c $(CCFLAGS) $(DEPFLAGS_CC) $< -o $@

shoreline: $(OBJS)
	$(CC) $(CCFLAGS) $^ $(DEPFLAGS_LD) -o shoreline

clean:
	$(RM) $(OBJS)
	$(RM) shoreline

.PHONY: all clean
