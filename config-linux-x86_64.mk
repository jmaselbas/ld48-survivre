LIBDIR = lib64
CFLAGS += -m64
LDFLAGS += -m64
LDFLAGS += -L$(LIBDIR) -Wl,-rpath=./$(LIBDIR) -rdynamic
