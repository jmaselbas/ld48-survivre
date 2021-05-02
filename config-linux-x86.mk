PKG_CONFIG_PATH = /usr/lib32/pkgconfig/
LIBDIR = lib32
CFLAGS += -m32
LDFLAGS += -m32
LDFLAGS += -L$(LIBDIR) -Wl,-rpath=./$(LIBDIR) -rdynamic
