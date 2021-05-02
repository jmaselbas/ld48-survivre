# See LICENSE file for copyright and license details.
.POSIX:

include config.mk

include engine/Makefile
include game/Makefile
include plat/Makefile

obj = $(src:.c=.o)
plt-obj = $(plt-src:.c=.o)
BIN ?= survivre
LIB = $(LIBDIR)/$(LIBNAME)
RES = res/audio/casey.ogg \
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
DLL = SDL2.dll

# dynlib is the default target for now, not meant for release
all: dynlib

static: CFLAGS += -DSTATIC
static: $(BIN);

# dynlib build enable game code hot reloading
dynlib: LDFLAGS += -ldl
dynlib: $(LIB) $(BIN);

$(LIB): $(obj)
	@mkdir -p $(dir $@)
	$(CC) -shared $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(BIN): main.o $(plt-obj) $(obj)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(BIN).x86_64: FORCE
	make clean
	make RELEASE=y static
	mv $(BIN) $@

$(BIN).x86: FORCE
	make clean
	make RELEASE=y TARGET=x86 static
	mv $(BIN) $@

$(BIN).exe: FORCE
	make clean
	make RELEASE=1 TARGET=w64 static

$(BIN).html: FORCE
	make clean
	make CROSS_COMPILE=em static

FORCE:;
.PHONY: FORCE

$(BIN)-$(VERSION).zip: $(BIN).exe $(BIN).x86 $(BIN).x86_64 $(DLL) $(RES)
	mkdir -p $(basename $@)
	for i in $^; do mkdir -p $(basename $@)/$$(dirname $$i)/ && cp $$i $(basename $@)/$$(dirname $$i)/; done
	zip -r $@ $(basename $@)

$(BIN).data $(BIN).js $(BIN).wasm: $(BIN).html;

$(BIN)-$(VERSION)-web.zip: $(BIN).html $(BIN).wasm $(BIN).data $(BIN).js
	mkdir -p $(basename $@)
	for i in $^; do mkdir -p "$(basename $@)"/$$(dirname $$i)/ && cp $$i "$(basename $@)"/$$(dirname $$i)/; done
	mv $(basename $@)/$(BIN).html $(basename $@)/$(dir $(BIN))/index.html
	zip -r $@ $(basename $@)

dist-web: $(BIN)-$(VERSION)-web.zip

dist: $(BIN)-$(VERSION).zip

dist-clean:
	rm -rf $(BIN)-$(VERSION) $(BIN)-$(VERSION)-www

clean:
	rm -f $(BIN) main.o $(obj) $(dep) $(plt-obj)

.PHONY: all static dynlib clean dist dist-web dist-clean

# namesubst perform a patsubst only on the file name, while keeping the path intact
# usage: $(call namesubst,pattern,replacement,text)
# example: $(call namesubst,%.c,.%.d,foo/aa.c bar/bb.c)
# produce: foo/.aa.d bar/.bb.d
define namesubst
	$(foreach i,$3,$(subst $(notdir $i),$(patsubst $1,$2,$(notdir $i)), $i))
endef

dep = $(call namesubst,%,.%.mk,$(src) $(plt-src))

# special rule to generate dependency rules for every object file (.o)
# mainly used to trigger recompilation on header file change
plat/.%.mk: plat/%
	$(CC) -E -MM -MT $(^:.c=.o) $^ -MF $@
game/.%.mk: game/%
	$(CC) -E -MM -MT $(^:.c=.o) $^ -MF $@
engine/.%.mk: engine/%
	$(CC) -E -MM -MT $(^:.c=.o) $^ -MF $@

-include $(dep)
