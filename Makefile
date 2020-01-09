CC ?= gcc
RM = rm -f
RMDIR = rm -rf

# Default: Enable all features that do not impact performance
FEATURES ?= SIZE OFFSET STATISTICS SDL NUMA VNC TTF FBDEV #PIXEL_COUNT

# Declare features compiled conditionally
CODE_FEATURES = STATISTICS SDL NUMA VNC TTF FBDEV

SOURCE_SDL = sdl.c
HEADER_SDL = sdl.h
DEPS_SDL = sdl2
CCFLAGS_sdl2 = -I/usr/include/SDL2 -D_REENTRANT
LDFLAGS_sdl2 = -lSDL2

SOURCE_VNC = vnc.c
HEADER_VNC = vnc.h
DEPS_VNC = libvncserver
LDFLAGS_libvncserver = -lvncserver

SOURCE_TTF = textrender.c
HEADER_TTF = textrender.h
DEPS_TTF = freetype2
CCFLAGS_freetype2 = -I/usr/include/freetype2 -I/usr/include/libpng16 -I/usr/include/harfbuzz -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include
LDFLAGS_freetype2 = -lfreetype

SOURCE_STATISTICS = statistics.c
HEADER_STATISTICS = statistics.h

SOURCE_FBDEV = linuxfb.c
HEADER_FBDEV = linuxfb.h

DEPS_NUMA = numa
LDFLAGS_numa = -lnuma

# Sanitize features
FEATURES := $(sort $(FEATURES))
BUILD_DIR = build/$(subst $() $(),-,$(FEATURES))

# Create lists of FEATURES components
FEATURE_SOURCES = $(foreach feature,$(CODE_FEATURES),$(SOURCE_$(feature)))
FEATURE_HEADERS = $(foreach feature,$(CODE_FEATURES),$(HEADER_$(feature)))
FEATURE_DEPS = $(foreach feature,$(CODE_FEATURES),$(DEPS_$(feature)))

# Set default compile flags
CCFLAGS = -Wall -D_GNU_SOURCE
ifneq ($(DEBUG),)
	CCFLAGS += -O1 -ggdb -DDEBUG=$(DEBUG)
	BUILD_DIR := $(BUILD_DIR)-$(DEBUG)
else
	CCFLAGS += -Ofast -march=native
endif
CCFLAGS += $(foreach feature,$(FEATURES),-DFEATURE_$(feature))

# Build dependency compile flags
DEPFLAGS_CC =
DEPS = $(filter $(FEATURE_DEPS),$(foreach feature,$(FEATURES),$(DEPS_$(feature))))
# Try fetching compile flags from pkg-config, use static ones on failure
DEPFLAGS_CC += $(foreach feature,$(FEATURES),\
	$(foreach dep,$(DEPS_$(feature)),\
		$(shell pkg-config --cflags $(dep) || ((1>&2 echo Missing pkg-config file for $(dep), trying $(CCFLAGS_$(dep)) && echo "$(CCFLAGS_$(dep))") ))))

# Build dependency linker flags
DEPFLAGS_LD = -lpthread
# Try fetching linker flags from pkg-config, use static ones on failure
DEPFLAGS_LD += $(foreach feature,$(FEATURES),\
	$(foreach dep,$(DEPS_$(feature)),\
		$(shell pkg-config --libs $(dep) || ((1>&2 echo Missing pkg-config file for $(dep), trying $(LDFLAGS_$(dep)) && echo "$(LDFLAGS_$(dep))") ))))

# Select source files
FEATURE_SOURCE = $(foreach feature,$(FEATURES),$(SOURCE_$feature))
SOURCE = $(filter-out $(FEATURE_SOURCES),$(wildcard *.c))
SOURCE += $(foreach feature,$(FEATURES),$(SOURCE_$(feature)))
OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCE))
HDRS = $(filter-out $(FEATURE_HEADERS),$(wildcard *.h))
HDRS += $(foreach feature,$(FEATURES),$(HEADER_$(feature)))

all: shoreline

$(BUILD_DIR)/%.o : %.c $(HDRS) Makefile | $(BUILD_DIR)
	$(CC) -c $(CCFLAGS) $(DEPFLAGS_CC) $< -o $@

shoreline: $(OBJS)
	$(CC) $(CCFLAGS) $^ $(DEPFLAGS_LD) -o shoreline

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	$(RMDIR) build
	$(RM) shoreline

.PHONY: all clean
