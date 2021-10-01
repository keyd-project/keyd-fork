.PHONY: all clean install uninstall

DESTDIR=
PREFIX=/usr

LOCK_FILE="/var/lock/keyd.lock"
LOG_FILE="/var/log/keyd.log"
CONFIG_DIR="/etc/keyd"

VERSION=1.1.2
GIT_HASH=$(shell git describe --no-match --always --abbrev=40 --dirty)

# for input.h input-event-codes.h uinput.h
CPPFLAGS=-idirafter /usr/include/linux

CONFIG=-DVERSION=\"$(VERSION)\" \
	-DGIT_COMMIT_HASH=\"$(GIT_HASH)\" \
	-DCONFIG_DIR=\"$(CONFIG_DIR)\" \
	-DLOG_FILE=\"$(LOG_FILE)\" \
	-DLOCK_FILE=\"$(LOCK_FILE)\"

all:
	mkdir -p bin
	$(CC) -O3 $(CONFIG) $(CPPFLAGS) $(LDFLAGS) src/*.c -o bin/keyd -ludev
man:
	pandoc -s -t man man.md | gzip > keyd.1.gz
clean:
	-rm -rf bin
install:
	mkdir -p $(DESTDIR)/etc/keyd
	mkdir -p $(DESTDIR)$(PREFIX)/lib/systemd/system
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(PREFIX)/share/man/man1

	install -m644 keyd.service $(DESTDIR)$(PREFIX)/lib/systemd/system
	install -m755 bin/keyd $(DESTDIR)$(PREFIX)/bin
	install -m644 keyd.1.gz $(DESTDIR)$(PREFIX)/share/man/man1
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/lib/systemd/system/keyd.service\
		bin/keyd $(DESTDIR)$(PREFIX)/bin/keyd\
		$(DESTDIR)$(PREFIX)/share/man/man1/keyd.1.gz

