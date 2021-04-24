# See LICENSE file for copyright and license details.
.POSIX:

include config.mk

include engine/Makefile
include game/Makefile
include plat/Makefile

obj = $(src:.c=.o)
plt-obj = $(plt-src:.c=.o)
BIN = survivre
LIB = $(LIBDIR)/$(LIBNAME)

# dynlib is the default target for now, not meant for release
all: dynlib

static: CFLAGS += -DSTATIC
static: $(BIN);

# dynlib build enable game code hot reloading
dynlib: LDFLAGS += -ldl
dynlib: $(LIB) $(BIN);

$(LIB): $(obj)
	$(CC) -shared $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(BIN): main.o $(plt-obj) $(obj)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(BIN) main.o $(obj) $(dep) $(plt-obj)

.PHONY: all static dynlib clean

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
