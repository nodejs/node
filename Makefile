#!/usr/bin/make -f

all:
	@tools/waf-light build

all-debug:
	@tools/waf-light -v build

all-progress:
	@tools/waf-light -p build

install:
	@tools/waf-light install

uninstall:
	@tools/waf-light uninstall
 
test: all
	python tools/test.py --mode=release
  
test-all: all
	python tools/test.py --mode=debug,release

test-debug: all
	python tools/test.py --mode=debug

benchmark: all
	build/default/node benchmark/run.js

doc: doc/node.1 doc/api.html doc/index.html

doc/api.html: doc/api.txt
	asciidoc --unsafe              \
		-a theme=pipe                \
		-a toc                       \
		-a linkcss                   \
		-o doc/api.html doc/api.txt

doc/api.xml: doc/api.txt
	asciidoc -b docbook -d manpage -o doc/api.xml doc/api.txt

doc/node.1: doc/api.xml
	xsltproc --output doc/node.1                \
		--nonet /etc/asciidoc/docbook-xsl/manpage.xsl \
		doc/api.xml

website-upload: doc
	scp doc/* linode:~/tinyclouds/node/

clean:
	@tools/waf-light clean

distclean:
	@tools/waf-light distclean
	@-rm -rf _build_
	@-rm -f Makefile
	@-rm -f *.pyc

check:
	@tools/waf-light check

dist:
	@tools/waf-light dist

.PHONY: benchmark clean dist distclean check uninstall install all test test-all website-upload
