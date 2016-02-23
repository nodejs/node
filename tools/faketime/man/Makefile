INSTALL ?= install

PREFIX ?= /usr/local

all:

install:
	$(INSTALL) -Dm0644 faketime.1 "${DESTDIR}${PREFIX}/share/man/man1/faketime.1"
	gzip -f "${DESTDIR}${PREFIX}/share/man/man1/faketime.1"

uninstall:
	rm -f "${DESTDIR}${PREFIX}/share/man/man1/faketime.1.gz"

.PHONY: all install uninstall
