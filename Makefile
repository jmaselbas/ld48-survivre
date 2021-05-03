# See LICENSE file for copyright and license details.
.POSIX:

# Makefile usage: make [option] [target]
# This makefile accept some specific options:
# O=<build-dir>        out-of-tree build dir, compilation objects will be there
# TARGET=<target>      targeted build system, available: linux-x86_64 linux-x86 w64 wasm
# DESTDIR=<dir>        destination dir, "fake" root directory for distributing an installation
# CROSS_COMPILE=<name> name prefix for an external toolchain, ex: arm-none-eabi-

include config.mk

include engine/Makefile
include game/Makefile
include plat/Makefile

ifneq ($(O),)
$(shell mkdir -p $(O))
OUT = $(shell basename $(O))/
endif

obj = $(addprefix $(OUT),$(src:.c=.o))
plt-src += main.c
plt-obj = $(addprefix $(OUT),$(plt-src:.c=.o))
BIN = survivre$(EXT)
LIB = $(LIBDIR)/libgame.so
RES += res/audio/casey.ogg \
 res/audio/fx_bip_01.wav \
 res/audio/fx_crash_01.wav \
 res/audio/fx_crash_02.wav \
 res/audio/fx_crash_03.wav \
 res/audio/fx_crash_04.wav \
 res/audio/fx_wind_loop.ogg \
 res/audio/fx_woosh_01.wav \
 res/audio/fx_woosh_02.wav \
 res/audio/fx_woosh_03.wav \
 res/audio/fx_woosh_04.wav \
 res/audio/LD48_loop_fade.ogg \
 res/cap.obj \
 res/menu_quit.obj \
 res/menu_start.obj \
 res/player.obj \
 res/rock.obj \
 res/room.obj \
 res/screen.obj \
 res/wall.obj \
 res/orth.vert \
 res/proj.vert \
 res/solid.frag \
 res/screen.frag \
 res/wall.frag

# dynlib is the default target for now, not meant for release
all: dynlib

static: $(OUT)$(BIN);

# dynlib build enable game code hot reloading
dynlib: LDFLAGS += -ldl
dynlib: CFLAGS += -DDYNAMIC_RELOAD
dynlib: $(OUT)$(LIB) $(OUT)$(BIN);

$(OUT)$(LIB): $(obj)
	@mkdir -p $(dir $@)
	$(CC) -shared $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(OUT)$(BIN): $(plt-obj) $(obj)
	@mkdir -p $(dir $@)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) $(LDLIBS)

$(OUT)%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(CFLAGS)
	@$(CC) -MP -MM $< -MT $@ -MF $(call namesubst,%,.%.mk,$@) $(CFLAGS)

install: $(OUT)$(BIN) $(RES)
	@mkdir -p $(DESTDIR)
	install $< $(DESTDIR)
# tar is used here to copy files while preserving the original path of each file
	tar cf - $(RES) | tar xf - -C $(DESTDIR)

clean:
	rm -f $(BIN) main.o $(obj) $(dep) $(plt-obj)

.PHONY: all static dynlib clean

include dist.mk

# namesubst perform a patsubst only on the file name, while keeping the path intact
# usage: $(call namesubst,pattern,replacement,text)
# example: $(call namesubst,%.c,.%.d,foo/aa.c bar/bb.c)
# produce: foo/.aa.d bar/.bb.d
define namesubst
	$(foreach i,$3,$(subst $(notdir $i),$(patsubst $1,$2,$(notdir $i)), $i))
endef

-include $(shell find . -name ".*.mk")
