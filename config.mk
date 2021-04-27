# game version
VERSION = 0.48.1

# Customize below to fit system
CONFIG_JACK=n
CONFIG_PULSE=n
CONFIG_MINIAUDIO=y

# Install paths
PREFIX := /usr/local
MANPREFIX := $(PREFIX)/share/man

ifeq ($(TARGET),w64)
CROSS_COMPILE = x86_64-w64-mingw32-
INCS := -Iglfw-3.3.2.bin.WIN64/include -Iglew/glew-2.2.0/include
LIBS := -Lglfw-3.3.2.bin.WIN64/lib-mingw-w64 -Lglew/glew-2.2.0/lib/Release/x64
LIBS += -lopengl32 -luser32 -lkernel32 -lgdi32 -lglu32 -lm -lglew32
LIBS += -lglfw3dll
CFLAGS += -DGLFW_DLL -DWINDOWS
# only static build is supported
LDFLAGS += --static -Wl,--no-undefined -static-libgcc
endif

ifeq ($(TARGET),x86)
PKG_CONFIG_PATH = /usr/lib32/pkgconfig/
CFLAGS += -m32
LDFLAGS += -m32
LIBDIR = .
LIBNAME = libgame.so
LDFLAGS += -L$(LIBDIR) -Wl,-rpath=./$(LIBDIR)
else ifeq ($(TARGET),)
LIBDIR = .
LIBNAME = libgame.so
LDFLAGS += -L$(LIBDIR) -Wl,-rpath=./$(LIBDIR)
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
LDFLAGS += -s USE_GLFW=3 -s WASM=0 -s MIN_WEBGL_VERSION=2 -s FULL_ES3=1
PKG = emconfigure pkg-config
endif

PKG_CONFIG_PATH ?= /usr/lib/pkgconfig/
PKG ?= PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config
PKG ?= pkg-config

# Depencies includes and libs
INCS ?= $(shell $(PKG) --cflags glfw3 glew)
LIBS ?= $(shell $(PKG) --libs glfw3 glew)
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
CFLAGS += -std=c99 -D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=200112L
CFLAGS += -O2 -W -fPIC -g -Wall
CFLAGS += $(INCS) -DVERSION=\"$(VERSION)\"

LDFLAGS += $(LIBS)
