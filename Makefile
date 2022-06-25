VERSION = 0.2.1-rev+${shell git rev-parse --short=16 HEAD}
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man
LDLIBS = -lxcb
LDFLAGS = -s ${LDLIBS}
INCS = -I/usr/include
CFLAGS = -std=c99 -pedantic -Wall -Wextra -Os ${INCS} -DVERSION="\"${VERSION}\""
CC = cc

SRC = src/xcpick.c \
	  src/debug.c

OBJ = ${SRC:.c=.o}

all: xcpick

${OBJ}:	src/debug.h \
		src/cursorfont.h

xcpick: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f xcpick ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/xcpick
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	cp -f man/xcpick.1 ${DESTDIR}${MANPREFIX}/man1
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/xcpick.1

dist: clean
	mkdir -p xcpick-${VERSION}
	cp -R LICENSE Makefile README man src xcpick-${VERSION}
	tar -cf xcpick-${VERSION}.tar xcpick-${VERSION}
	gzip xcpick-${VERSION}.tar
	rm -rf xcpick-${VERSION}

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/xcpick
	rm -f ${DESTDIR}${MANPREFIX}/man1/xcpick.1

clean:
	rm -f xcpick xcpick-${VERSION}.tar.gz ${OBJ}

.PHONY: all clean install uninstall dist
