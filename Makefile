WAF=python tools/waf-light

all:
	@$(WAF) build

all-progress:
	@$(WAF) -p build

install:
	@$(WAF) install

uninstall:
	@$(WAF) uninstall

test: all
	python tools/test.py --mode=release simple message

test-all: all
	python tools/test.py --mode=debug,release

test-release: all
	python tools/test.py --mode=release

test-debug: all
	python tools/test.py --mode=debug

test-message: all
	python tools/test.py message

test-simple: all
	python tools/test.py simple
     
test-pummel: all
	python tools/test.py pummel
	
test-internet: all
	python tools/test.py internet

benchmark: all
	build/default/node benchmark/run.js

# http://rtomayko.github.com/ronn
# gem install ronn
doc: doc/node.1 doc/api.html doc/index.html doc/changelog.html

## HACK to give the ronn-generated page a TOC
doc/api.html: all doc/api.markdown doc/api_header.html doc/api_footer.html
	build/default/node tools/ronnjs/bin/ronn.js --fragment doc/api.markdown \
	| sed "s/<h2>\(.*\)<\/h2>/<h2 id=\"\1\">\1<\/h2>/g" \
	| cat doc/api_header.html - doc/api_footer.html > doc/api.html

doc/changelog.html: ChangeLog doc/changelog_header.html doc/changelog_footer.html
	cat doc/changelog_header.html ChangeLog doc/changelog_footer.html > doc/changelog.html

doc/node.1: doc/api.markdown all
	build/default/node tools/ronnjs/bin/ronn.js --roff doc/api.markdown > doc/node.1

website-upload: doc
	scp doc/* ryan@nodejs.org:~/tinyclouds/node/

docclean:
	@-rm -f doc/node.1 doc/api.html doc/changelog.html

clean:
	@$(WAF) clean
	@-find tools -name "*.pyc" | xargs rm -f

distclean: docclean
	@-find tools -name "*.pyc" | xargs rm -f
	@-rm -rf build/ node node_g

check:
	@tools/waf-light check

VERSION=$(shell git describe)
TARNAME=node-$(VERSION)

dist: doc/node.1 doc/api.html
	git archive --format=tar --prefix=$(TARNAME)/ HEAD | tar xf -
	mkdir -p $(TARNAME)/doc
	cp doc/node.1 $(TARNAME)/doc/node.1
	cp doc/api.html $(TARNAME)/doc/api.html
	rm -rf $(TARNAME)/deps/v8/test # too big
	tar -cf $(TARNAME).tar $(TARNAME)
	rm -rf $(TARNAME)
	gzip -f -9 $(TARNAME).tar

.PHONY: benchmark clean docclean dist distclean check uninstall install all test test-all website-upload
