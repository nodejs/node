INSTALL ?= install

UNAME=$(shell uname)
SELECTOR:=$(shell if test "${UNAME}" = "Darwin" ; then echo "-f Makefile.OSX" ; fi)

all:
	$(MAKE) $(SELECTOR) -C src all

test:
	$(MAKE) $(SELECTOR) -C test all

install:
	$(MAKE) $(SELECTOR) -C src install
	$(MAKE) $(SELECTOR) -C man install
	$(INSTALL) -dm0755 "${DESTDIR}${PREFIX}/share/doc/faketime/"
	$(INSTALL) -m0644 README "${DESTDIR}${PREFIX}/share/doc/faketime/README"
	$(INSTALL) -m0644 NEWS "${DESTDIR}${PREFIX}/share/doc/faketime/NEWS"

uninstall:
	$(MAKE) $(SELECTOR) -C src uninstall
	$(MAKE) $(SELECTOR) -C man uninstall
	rm -f "${DESTDIR}${PREFIX}/share/doc/faketime/README"
	rm -f "${DESTDIR}${PREFIX}/share/doc/faketime/NEWS"
	rmdir "${DESTDIR}${PREFIX}/share/doc/faketime"

clean:
	$(MAKE) $(SELECTOR) -C src clean
	$(MAKE) $(SELECTOR) -C test clean

distclean:
	$(MAKE) $(SELECTOR) -C src distclean
	$(MAKE) $(SELECTOR) -C test distclean

.PHONY: all test install uninstall clean distclean
