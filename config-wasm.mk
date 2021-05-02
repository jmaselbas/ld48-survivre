CROSS_COMPILE ?= em
CFLAGS += -s USE_SDL=2
LDFLAGS += -s USE_SDL=2 -s USE_WEBGL2=1 -s FULL_ES3=1
LDFLAGS += -s ASSERTIONS=1 -s TOTAL_MEMORY=$$(( 8 * 64 * 1024 * 1024 ))
LDFLAGS += $(foreach r,$(RES),--preload-file $(r))
PKG_CONFIG_PATH = SDL2-2.0.14/x86_64-w64-mingw32/lib/pkgconfig/
PKG = emconfigure pkg-config
PKG = : # disable PKG since it doesn't work right now

# Overwrite installation resources files, as resources are already
# built-in the output binary with --preload-file option
install-web: WEB=$(OUT)$(BIN) $(addprefix $(OUT)$(basename $(BIN)), .wasm .data .js)
install-web: static
	@mkdir -p $(DESTDIR)
	cp $(WEB) $(DESTDIR)
	mv $(DESTDIR)/$(BIN) $(DESTDIR)/index.html

.PHONY: install-web
