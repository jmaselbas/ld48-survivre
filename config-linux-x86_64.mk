LIBDIR = lib64
LDFLAGS += -L$(LIBDIR) -Wl,-rpath=./$(LIBDIR) -rdynamic
