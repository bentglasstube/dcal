# dcal - dynamic menu
# See LICENSE file for copyright and license details.

include config.mk

SRC = dcal.c draw.c 
OBJ = ${SRC:.c=.o}

all: options dcal

options:
	@echo dcal build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC -c $<
	@${CC} -c $< ${CFLAGS}

${OBJ}: config.mk draw.h

dcal: dcal.o draw.o
	@echo CC -o $@
	@${CC} -o $@ dcal.o draw.o ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f dcal ${OBJ} dcal-${VERSION}.tar.gz

dist: clean
	@echo creating dist tarball
	@mkdir -p dcal-${VERSION}
	@cp LICENSE Makefile README config.mk dcal.1 draw.h ${SRC} dcal-${VERSION}
	@tar -cf dcal-${VERSION}.tar dcal-${VERSION}
	@gzip dcal-${VERSION}.tar
	@rm -rf dcal-${VERSION}

install: all
	@echo installing executables to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f dcal ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/dcal
	@echo installing manual pages to ${DESTDIR}${MANPREFIX}/man1
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@sed "s/VERSION/${VERSION}/g" < dcal.1 > ${DESTDIR}${MANPREFIX}/man1/dcal.1
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/dcal.1

uninstall:
	@echo removing executables from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/dcal
	@echo removing manual page from ${DESTDIR}${MANPREFIX}/man1
	@rm -f ${DESTDIR}${MANPREFIX}/man1/dcal.1

.PHONY: all options clean dist install uninstall
