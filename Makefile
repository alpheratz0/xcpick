.POSIX:
.PHONY: all clean install uninstall dist

include config.mk

all: xcpick

xcpick: xcpick.o
	$(CC) $(LDFLAGS) -o xcpick xcpick.o

clean:
	rm -f xcpick xcpick.o xcpick-$(VERSION).tar.gz

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f xcpick $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/xcpick
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	cp -f xcpick.1 $(DESTDIR)$(MANPREFIX)/man1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/xcpick.1

dist: clean
	mkdir -p xcpick-$(VERSION)
	cp -R COPYING config.mk Makefile README xcpick.1 xcpick.c xcpick-$(VERSION)
	tar -cf xcpick-$(VERSION).tar xcpick-$(VERSION)
	gzip xcpick-$(VERSION).tar
	rm -rf xcpick-$(VERSION)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/xcpick
	rm -f $(DESTDIR)$(MANPREFIX)/man1/xcpick.1
