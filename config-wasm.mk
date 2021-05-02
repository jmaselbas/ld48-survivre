CROSS_COMPILE ?= em
CFLAGS += -s USE_SDL=2
LDFLAGS += -s USE_SDL=2 -s USE_WEBGL2=1 -s FULL_ES3=1
LDFLAGS += -s ASSERTIONS=1 -s TOTAL_MEMORY=$$(( 8 * 64 * 1024 * 1024 ))
LDFLAGS += $(foreach r,$(RES),--preload-file $(r))
PKG_CONFIG_PATH = SDL2-2.0.14/x86_64-w64-mingw32/lib/pkgconfig/
PKG = emconfigure pkg-config
PKG = : # disable PKG since it doesn't work right now
