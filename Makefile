WAF=python tools/waf-light

all: program

all-progress:
	@$(WAF) -p build

program:
	@$(WAF) --product-type=program build

staticlib:
	@$(WAF) --product-type=cstaticlib build

dynamiclib:
	@$(WAF) --product-type=cshlib build

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


doc: doc/api/all.html doc/changelog.html

docopen: doc/api/all.html
	-google-chrome doc/api/all.html

doc/api/all.html: node  doc/api/*.markdown
	./node tools/doctool/doctool.js

doc/changelog.html: ChangeLog doc/changelog_header.html doc/changelog_footer.html
	cat doc/changelog_header.html ChangeLog doc/changelog_footer.html > doc/changelog.html

website-upload: doc
	scp doc/* ryan@nodejs.org:~/web/nodejs.org/

docclean:
	@-rm -f doc/node.1 doc/api/*.html doc/changelog.html

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

bench:
	 benchmark/http_simple_bench.sh

bench-idle:
	./node benchmark/idle_server.js &
	sleep 1
	./node benchmark/idle_clients.js &


.PHONY: bench clean docclean dist distclean check uninstall install all program staticlib dynamiclib test test-all website-upload
