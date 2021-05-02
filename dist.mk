# This file contain build rules for each targeted platform

DIST_TARGET = dist-linux-x86_64 dist-linux-x86 dist-w64
DIST_WEB_TARGET = dist-wasm

dist: DIST_ACTION=install
dist: $(BIN)-$(VERSION).zip;

$(BIN)-$(VERSION).zip: DESTDIR=$(BIN)-$(VERSION)
$(BIN)-$(VERSION).zip: $(DIST_TARGET)
	zip -r $@ $(basename $@)

dist-web: DIST_ACTION=install-web
dist-web: $(BIN)-$(VERSION)-web.zip;

$(BIN)-$(VERSION)-web.zip: DESTDIR=$(BIN)-$(VERSION)-web
$(BIN)-$(VERSION)-web.zip: dist-wasm
	zip -r $@ $(basename $@)

dist-linux-x86_64:
	make O=build_x86_64 TARGET=linux-x86_64 EXT=.x86_64 DESTDIR=$(DESTDIR) RELEASE=y $(DIST_ACTION)

dist-linux-x86:
	make O=build_x86 TARGET=linux-x86 EXT=.x86 DESTDIR=$(DESTDIR) RELEASE=y $(DIST_ACTION)

dist-w64:
	make O=build_w64 TARGET=w64 EXT=.exe DESTDIR=$(DESTDIR) RELEASE=y $(DIST_ACTION)

dist-wasm:
	make O=build_wasm TARGET=wasm EXT=.html DESTDIR=$(DESTDIR) RELEASE=y $(DIST_ACTION)

dist-clean: DIST_ACTION=clean
dist-clean: $(DIST_TARGET) $(DIST_WEB_TARGET)
	rm -rf $(BIN)-$(VERSION) $(BIN)-$(VERSION)-web

.PHONY: dist dist-web dist-clean $(DIST_TARGET) $(DIST_WEB_TARGET)
