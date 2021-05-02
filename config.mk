# game version
VERSION = 0.48.3

# Customize below to fit system
CONFIG_JACK=n
CONFIG_PULSE=n
CONFIG_MINIAUDIO=n
CONFIG_SDL_AUDIO=y

# Install paths
PREFIX := /usr/local
MANPREFIX := $(PREFIX)/share/man

ifeq ($(TARGET),w64)
CROSS_COMPILE = x86_64-w64-mingw32-
INCS := -ISDL2-2.0.14/x86_64-w64-mingw32/include/SDL2
LIBS := -LSDL2-2.0.14/x86_64-w64-mingw32/lib
LIBS += -lmingw32 -lSDL2main -lSDL2
LIBS += -Wl,-Bstatic -lpthread -lm -Wl,-Bdynamic
CFLAGS += -DWINDOWS
LDFLAGS += -Wl,--no-undefined -static-libgcc
endif

ifeq ($(TARGET),x86)
PKG_CONFIG_PATH = /usr/lib32/pkgconfig/
CFLAGS += -m32
LDFLAGS += -m32
LIBDIR = lib32
LIBNAME = libgame.so
LDFLAGS += -L$(LIBDIR) -Wl,-rpath=./$(LIBDIR) -rdynamic
else ifeq ($(TARGET),)
LIBDIR = lib64
LIBNAME = libgame.so
LDFLAGS += -L$(LIBDIR) -Wl,-rpath=./$(LIBDIR) -rdynamic
endif

ifneq ($(CROSS_COMPILE),)
CC      = $(CROSS_COMPILE)cc
LD      = $(CROSS_COMPILE)ld
AR      = $(CROSS_COMPILE)ar
NM      = $(CROSS_COMPILE)nm
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump
READELF = $(CROSS_COMPILE)readelf
OBJSIZE = $(CROSS_COMPILE)size
STRIP   = $(CROSS_COMPILE)strip
endif

ifeq ($(CC),emcc)
CFLAGS += -s USE_SDL=2
LDFLAGS += -s USE_SDL=2 -s USE_WEBGL2=1 -s FULL_ES3=1
LDFLAGS += -s ASSERTIONS=1 -s TOTAL_MEMORY=$$(( 8 * 64 * 1024 * 1024 ))
LDFLAGS += $(foreach r,$(RES),--preload-file $(r))
BIN = survivre.html
PKG = emconfigure pkg-config
endif

PKG_CONFIG_PATH ?= /usr/lib/pkgconfig/
PKG ?= PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config
PKG ?= pkg-config

# Depencies includes and libs
INCS ?= $(shell $(PKG) --cflags sdl2)
LIBS ?= $(shell $(PKG) --libs sdl2)
LIBS += -lm

ifeq ($(CONFIG_JACK),y)
LIBS += -lpthread -ljack
CFLAGS += -DCONFIG_JACK
endif
ifeq ($(CONFIG_PULSE),y)
LIBS += -lpthread -lpulse -lpulse-simple
CFLAGS += -DCONFIG_PULSE
endif
ifeq ($(CONFIG_MINIAUDIO),y)
LIBS += -lpthread
ifneq ($(TARGET),w64)
LIBS += -ldl
endif
CFLAGS += -DCONFIG_MINIAUDIO
endif

# Flags
CFLAGS += -D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=200112L
CFLAGS += -O2 -W -fPIC -Wall -g
ifneq ($(RELEASE),)
CFLAGS += -s -ffunction-sections
endif
CFLAGS += $(INCS) -DVERSION=\"$(VERSION)\" -DCONFIG_SDL_AUDIO
LDFLAGS += $(LIBS)
