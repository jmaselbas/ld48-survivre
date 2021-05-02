CROSS_COMPILE = x86_64-w64-mingw32-
INCS := -ISDL2-2.0.14/x86_64-w64-mingw32/include/SDL2
LIBS := -LSDL2-2.0.14/x86_64-w64-mingw32/lib
LIBS += -lmingw32 -lSDL2main -lSDL2
LIBS += -Wl,-Bstatic -lpthread -lm -Wl,-Bdynamic
CFLAGS += -DWINDOWS
LDFLAGS += -Wl,--no-undefined -static-libgcc
RES += SDL2.dll
