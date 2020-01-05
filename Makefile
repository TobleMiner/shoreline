CC ?= gcc
RM = rm -f

FEATURES ?= SIZE OFFSET STATISTICS SDL NUMA VNC TTF FBDEV PIXEL_COUNT

SOURCE_SDL = sdl.c
HEADER_SDL = sdl.h
DEPS_SDL = sdl2

SOURCE_VNC = vnc.c
HEADER_VNC = vnc.h
DEPS_VNC = libvncserver

SOURCE_TTF = textrender.c
HEADER_TTF = textrender.h
DEPS_TTF = freetype2

SOURCE_STATISTICS = statistics.c
HEADER_STATISTICS = statistics.h

SOURCE_FBDEV = linuxfb.c
HEADER_FBDEV = linuxfb.h

DEPS_NUMA = numa

CODE_FEATURES = STATISTICS SDL NUMA VNC TTF FBDEV
FEATURE_SOURCES = $(foreach feature,$(CODE_FEATURES),$(SOURCE_$(feature)))
FEATURE_HEADERS = $(foreach feature,$(CODE_FEATURES),$(HEADER_$(feature)))
FEATURE_DEPS = $(foreach feature,$(CODE_FEATURES),$(DEPS_$(feature)))

CCFLAGS = -Wall -D_GNU_SOURCE
ifeq ($(DEBUG),1)
	CCFLAGS += -O1 -ggdb
else
	CCFLAGS += -Ofast -march=native
endif
CCFLAGS += $(foreach feature,$(FEATURES),-DFEATURE_$(feature))

DEPS = $(filter $(FEATURE_DEPS),$(foreach feature,$(FEATURES),$(DEPS_$(feature))))
DEPFLAGS_CC =
DEPFLAGS_LD = -lpthread
ifneq ($(DEPS),)
DEPFLAGS_CC += `pkg-config --cflags $(DEPS)`
DEPFLAGS_LD += `pkg-config --libs $(DEPS)`
endif
FEATURE_SOURCE = $(foreach feature,$(FEATURES),$(SOURCE_$feature))
SOURCE = $(filter-out $(FEATURE_SOURCES),$(wildcard *.c))
SOURCE += $(foreach feature,$(FEATURES),$(SOURCE_$(feature)))
OBJS = $(patsubst %.c,%.o,$(SOURCE))
HDRS = $(filter-out $(FEATURE_HEADERS),$(wildcard *.h))
HDRS += $(foreach feature,$(FEATURES),$(HEADER_$(feature)))

all: shoreline

%.o : %.c $(HDRS) Makefile
	$(CC) -c $(CCFLAGS) $(DEPFLAGS_CC) $< -o $@

shoreline: $(OBJS)
	$(CC) $(CCFLAGS) $^ $(DEPFLAGS_LD) -o shoreline

clean:
	$(RM) $(OBJS)
	$(RM) shoreline

.PHONY: all clean
