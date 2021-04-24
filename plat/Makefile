plt-src-y += core.c audio.c
plt-src-$(CONFIG_JACK)  += jack.c
plt-src-$(CONFIG_PULSE) += pulse.c
plt-src += $(patsubst %, plat/%, $(plt-src-y))
