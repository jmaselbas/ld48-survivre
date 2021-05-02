PKG_CONFIG_PATH = /usr/lib32/pkgconfig/
CFLAGS += -m32
LDFLAGS += -m32
LIBDIR = lib32
LDFLAGS += -L$(LIBDIR) -Wl,-rpath=./$(LIBDIR) -rdynamic
