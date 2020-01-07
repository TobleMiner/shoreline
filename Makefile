CC ?= gcc
RM = rm -f

# Default: Enable all features
FEATURES ?= SIZE OFFSET STATISTICS SDL NUMA VNC TTF FBDEV PIXEL_COUNT

# Declare features compiled conditionally
CODE_FEATURES = STATISTICS SDL NUMA VNC TTF FBDEV

SOURCE_SDL = sdl.c
HEADER_SDL = sdl.h
DEPS_SDL = sdl2
DEPFLAGS_sdl2 = SDL2

SOURCE_VNC = vnc.c
HEADER_VNC = vnc.h
DEPS_VNC = libvncserver
DEPFLAGS_libvncserver = vncserver

SOURCE_TTF = textrender.c
HEADER_TTF = textrender.h
DEPS_TTF = freetype2
DEPFLAGS_freetype2 = freetype

SOURCE_STATISTICS = statistics.c
HEADER_STATISTICS = statistics.h

SOURCE_FBDEV = linuxfb.c
HEADER_FBDEV = linuxfb.h

DEPS_NUMA = numa
DEPFLAGS_numa = numa

# Create lists of features components
FEATURE_SOURCES = $(foreach feature,$(CODE_FEATURES),$(SOURCE_$(feature)))
FEATURE_HEADERS = $(foreach feature,$(CODE_FEATURES),$(HEADER_$(feature)))
FEATURE_DEPS = $(foreach feature,$(CODE_FEATURES),$(DEPS_$(feature)))

# Set default compile features
CCFLAGS = -Wall -D_GNU_SOURCE
ifeq ($(DEBUG),1)
	CCFLAGS += -O1 -ggdb
else
	CCFLAGS += -Ofast -march=native
endif
CCFLAGS += $(foreach feature,$(FEATURES),-DFEATURE_$(feature))

# Build dependency compile flags
DEPFLAGS_CC =
DEPS = $(filter $(FEATURE_DEPS),$(foreach feature,$(FEATURES),$(DEPS_$(feature))))
ifneq ($(DEPS),)
DEPFLAGS_CC += `pkg-config --cflags $(DEPS)`
endif

# Build dependency linker flags
DEPFLAGS_LD = -lpthread
# Try fetching linker flags from pkg-config, use static ones on failure
DEPFLAGS_LD += $(foreach feature,$(FEATURES),$(foreach dep,$(DEPS_$(feature)),$(shell pkg-config --libs $(dep) || (1>&2 echo Missing pkg-config file for $(dep), trying -l$(DEPFLAGS_$(dep)) && echo "-l$(DEPFLAGS_$(dep))" ))))

# Select source files
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
