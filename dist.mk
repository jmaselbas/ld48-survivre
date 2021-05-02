DIST_TARGET=linux-x86_64 linux-x86 w64
DIST_WEB_TARGET=wasm

dist-linux-x86_64:
	make O=build_x86_64 TARGET=linux-x86_64 EXT=.x86_64 DESTDIR=$(DESTDIR) RELEASE=y $(DIST_ACTION)

dist-linux-x86:
	make O=build_x86 TARGET=linux-x86 EXT=.x86 DESTDIR=$(DESTDIR) RELEASE=y $(DIST_ACTION)

dist-w64:
	make O=build_w64 TARGET=w64 EXT=.exe DESTDIR=$(DESTDIR) RELEASE=y $(DIST_ACTION)

dist-wasm:
	make O=build_wasm TARGET=wasm EXT=.html DESTDIR=$(DESTDIR) RELEASE=y $(DIST_ACTION)

$(BIN)-$(VERSION).zip: DESTDIR:=$(BIN)-$(VERSION)
$(BIN)-$(VERSION).zip: $(addprefix dist-,$(DIST_TARGET));

$(BIN)-$(VERSION)-web.zip: DESTDIR:=$(BIN)-$(VERSION)-web
$(BIN)-$(VERSION)-web.zip: dist-wasm;

dist-web: DIST_ACTION=install-web
dist-web: $(BIN)-$(VERSION)-web.zip;

dist: DIST_ACTION=install
dist: $(BIN)-$(VERSION).zip;

dist-clean: DIST_ACTION=clean
dist-clean: $(addprefix dist-,$(DIST_TARGET) $(DIST_WEB_TARGET))
	rm -rf $(BIN)-$(VERSION) $(BIN)-$(VERSION)-web

.PHONY: dist dist-web dist-clean $(addprefix dist-,$(DIST_TARGET) $(DIST_WEB_TARGET))
