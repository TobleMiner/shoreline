CC ?= gcc
OBJCOPY ?= objcopy
RM = rm -f

CCFLAGS = -Wall -D_GNU_SOURCE
ifeq ($(DEBUG),1)
	CCFLAGS += -O1 -ggdb
else
	CCFLAGS += -Ofast -march=native
endif

DEPFLAGS_CC =
DEPFLAGS_LD = -lpthread
OBJS = ring.o llist.o framebuffer.o network.o main.o workqueue.o frontend.o

undefine PREPARE
undefine FEATURES

FEATURE_NUMA = no, install libnuma for NUMA support
CONFIG_NUMA ?= $(shell pkg-config --exists numa && echo 1)
ifeq ($(CONFIG_NUMA),1)
	FEATURE_NUMA = yes
	FEATURES += numa
	DEPFLAGS_CC += -DSHORELINE_NUMA
	DEPFLAGS_LD += -lnuma
else
endif

FEATURE_SDL = no, install sdl2 for sdl support
CONFIG_SDL ?= $(shell pkg-config --exists sdl2 && echo 1)
ifeq ($(CONFIG_SDL),1)
	FEATURE_SDL = yes
	FEATURES += sdl
	DEPFLAGS_CC += -DSHORELINE_SDL $(shell pkg-config --cflags sdl2)
	DEPFLAGS_LD += $(shell pkg-config --libs sdl2)
	OBJS += sdl.o
endif

FEATURE_VNC = no, install libvncserver for VNC server support
CONFIG_VNC ?= $(shell pkg-config --exists libvncserver && echo 1)
ifeq ($(CONFIG_VNC),1)
	FEATURE_VNC = yes
	FEATURES += vnc
	DEPFLAGS_CC += -DSHORELINE_VNC $(shell pkg-config --cflags libvncserver)
	DEPFLAGS_LD += $(shell pkg-config --libs libvncserver)
	OBJS += vnc.o
endif

ifeq ($(shell [ -f shoreline ]; echo $$?),0)
	OLDFEATURES = $(shell $(OBJCOPY) --dump-section .features=features shoreline 2> /dev/null && cat features)
else
	OLDFEATURES = $(FEATURES)
endif

ifneq ($(OLDFEATURES),$(FEATURES))
	PREPARE += clean
endif

all: $(PREPARE) features shoreline

%.o : %.c
	$(CC) -c $(CCFLAGS) $(DEPFLAGS_CC) $< -o $@

shoreline: $(OBJS)
	$(CC) $(CCFLAGS) $^ $(DEPFLAGS_LD) -o shoreline
	$(OBJCOPY) --update-section .features=features shoreline

features:
	$(info NUMA support = $(FEATURE_NUMA))
	$(info SDL support = $(FEATURE_SDL))
	$(info VNC server support = $(FEATURE_VNC))
	$(info Enabled features: $(FEATURES))
	echo -en $(FEATURES)\\0 > features

clean:
	$(RM) $(OBJS)
	$(RM) shoreline
	$(RM) features

.PHONY: clean all
