.PHONY: all clean install uninstall debug man compose test-harness
VERSION=2.4.3
COMMIT=$(shell git describe --no-match --always --abbrev=7 --dirty)
VKBD=uinput
PREFIX=/usr

CONFIG_DIR=/etc/keyd
SOCKET_PATH=/var/run/keyd.socket

CFLAGS:=-DVERSION=\"v$(VERSION)\ \($(COMMIT)\)\" \
	-I/usr/local/include \
	-L/usr/local/lib \
	-Wall \
	-Wextra \
	-Wno-unused \
	-std=c11 \
	-DSOCKET_PATH=\"$(SOCKET_PATH)\" \
	-DCONFIG_DIR=\"$(CONFIG_DIR)\" \
	-DDATA_DIR=\"$(PREFIX)/share/keyd\" \
	-D_FORTIFY_SOURCE=2 \
	-D_DEFAULT_SOURCE \
	-Werror=format-security \
	$(CFLAGS)

platform=$(shell uname -s)

ifeq ($(platform), Linux)
	COMPAT_FILES=
else
	LDFLAGS+=-linotify
	COMPAT_FILES=
endif

all:
	-mkdir bin
	cp scripts/keyd-application-mapper bin/
	$(CC) $(CFLAGS) -O3 $(COMPAT_FILES) src/*.c src/vkbd/$(VKBD).c -lpthread -o bin/keyd $(LDFLAGS)
debug:
	CFLAGS="-g -Wunused" $(MAKE)
compose:
	-mkdir data
	./scripts/generate_xcompose
man:
	for f in docs/*.scdoc; do \
		target=$${f%%.scdoc}.1.gz; \
		target=data/$${target##*/}; \
		scdoc < "$$f" | gzip > "$$target"; \
	done
install:
# First check if DESTDIR already has the systemd
# directory. Then if it exists under the system's base
# PREFIX (e.g. /usr/local), and finally check the
# standard systemd location in /usr.

	@if [ -e $(DESTDIR)$(PREFIX)/lib/systemd/ -o \
	      -e ${PREFIX}/lib/systemd/ -o \
	      -e /usr/lib/systemd ]; \
	then \
		install -Dm644 keyd.service $(DESTDIR)$(PREFIX)/lib/systemd/system/keyd.service; \
	else \
		@echo "$$(tput bold)$$(tput setaf 1)warning: systemd not found, you will need to manually add keyd to your system's init process.$$(tput sgr0)"; \
	fi

	@if [ "$(VKBD)" = "usb-gadget" ]; then \
		install -Dm644 src/vkbd/usb-gadget.service $(DESTDIR)$(PREFIX)/lib/systemd/system/keyd-usb-gadget.service; \
		install -Dm755 src/vkbd/usb-gadget.sh $(DESTDIR)$(PREFIX)/bin/keyd-usb-gadget.sh; \
	fi

	mkdir -p $(DESTDIR)$(CONFIG_DIR)
	mkdir -p $(DESTDIR)$(PREFIX)/bin/
	mkdir -p $(DESTDIR)$(PREFIX)/share/keyd/
	mkdir -p $(DESTDIR)$(PREFIX)/share/keyd/layouts/
	mkdir -p $(DESTDIR)$(PREFIX)/share/man/man1/
	mkdir -p $(DESTDIR)$(PREFIX)/share/doc/keyd/
	mkdir -p $(DESTDIR)$(PREFIX)/share/doc/keyd/examples/

ifeq ($(shell id -u), 0)
	-groupadd keyd
else
	@echo "$$(tput bold)$$(tput setaf 1)warning: Use 'groupadd keyd' or your distribution's equivalent as root to add the access group.$$(tput sgr0)"
endif
	install -m755 bin/* $(DESTDIR)$(PREFIX)/bin/
	install -m644 docs/*.md $(DESTDIR)$(PREFIX)/share/doc/keyd/
	install -m644 examples/* $(DESTDIR)$(PREFIX)/share/doc/keyd/examples/
	install -m644 layouts/* $(DESTDIR)$(PREFIX)/share/keyd/layouts
	install -m644 data/*.1.gz $(DESTDIR)$(PREFIX)/share/man/man1/
	install -m644 data/keyd.compose $(DESTDIR)$(PREFIX)/share/keyd/

uninstall:
	rm -rf $(DESTDIR)$(PREFIX)/lib/systemd/system/keyd.service \
		$(DESTDIR)$(PREFIX)/bin/keyd \
		$(DESTDIR)$(PREFIX)/bin/keyd-application-mapper \
		$(DESTDIR)$(PREFIX)/share/doc/keyd/ \
		$(DESTDIR)$(PREFIX)/share/man/man1/keyd*.gz \
		$(DESTDIR)$(PREFIX)/lib/systemd/system/keyd-usb-gadget.service \
		$(DESTDIR)$(PREFIX)/bin/keyd-usb-gadget.sh
clean:
	-rm -rf bin
test:
	@cd t; \
	for f in *.sh; do \
		./$$f; \
	done
test-io:
	-mkdir bin
	$(CC) \
	-DDATA_DIR= \
	-o bin/test-io \
		t/test-io.c \
		src/keyboard.c \
		src/string.c \
		src/macro.c \
		src/config.c \
		src/log.c \
		src/ini.c \
		src/keys.c  \
		src/unicode.c && \
	./bin/test-io t/test.conf t/*.t
