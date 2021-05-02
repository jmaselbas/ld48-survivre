CFLAGS += -Iplat/
plt-src-y += core.c glad.c audio.c
plt-src-$(CONFIG_JACK)  += jack.c
plt-src-$(CONFIG_PULSE) += pulse.c
plt-src-$(CONFIG_MINIAUDIO) += miniaudio.c miniaudio_imp.c
plt-src-$(CONFIG_SDL_AUDIO) += audio_sdl.c
plt-src += $(patsubst %, plat/%, $(plt-src-y))
