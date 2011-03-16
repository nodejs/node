WAF=python tools/waf-light

web_root = ryan@nodejs.org:~/web/nodejs.org/

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

test-valgrind: all
	python tools/test.py --mode=release --valgrind simple message

test-all: all
	python tools/test.py --mode=debug,release

test-all-valgrind: all
	python tools/test.py --mode=debug,release --valgrind

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

build/default/node: all

apidoc_sources = $(wildcard doc/api/*.markdown)
apidocs = $(addprefix build/,$(apidoc_sources:.markdown=.html))

apidoc_dirs = build/doc build/doc/api/ build/doc/api/assets

apiassets = $(subst api_assets,api/assets,$(addprefix build/,$(wildcard doc/api_assets/*)))

website_files = \
	build/doc/index.html    \
	build/doc/v0.4_announcement.html   \
	build/doc/cla.html      \
	build/doc/sh_main.js    \
	build/doc/sh_javascript.min.js \
	build/doc/sh_vim-dark.css \
	build/doc/logo.png      \
	build/doc/sponsored.png \
  build/doc/favicon.ico   \
	build/doc/pipe.css

doc: build/default/node $(apidoc_dirs) $(website_files) $(apiassets) $(apidocs)

$(apidoc_dirs):
	mkdir -p $@

build/doc/api/assets/%: doc/api_assets/% build/doc/api/assets/
	cp $< $@

build/doc/%: doc/%
	cp $< $@

build/doc/api/%.html: doc/api/%.markdown build/default/node $(apidoc_dirs) $(apiassets) tools/doctool/doctool.js
	build/default/node tools/doctool/doctool.js doc/template.html $< > $@

build/doc/%:

website-upload: doc
	scp -r build/doc/* $(web_root)

docopen: build/doc/api/all.html
	-google-chrome build/doc/api/all.html

docclean:
	-rm -rf build/doc

clean:
	$(WAF) clean
	-find tools -name "*.pyc" | xargs rm -f

distclean: docclean
	-find tools -name "*.pyc" | xargs rm -f
	-rm -rf build/ node node_g

check:
	@tools/waf-light check

VERSION=$(shell git describe)
TARNAME=node-$(VERSION)

#dist: doc/node.1 doc/api
dist: doc
	git archive --format=tar --prefix=$(TARNAME)/ HEAD | tar xf -
	mkdir -p $(TARNAME)/doc
	cp doc/node.1 $(TARNAME)/doc/node.1
	cp -r build/doc/api $(TARNAME)/doc/api
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

jslint:
	PYTHONPATH=tools/closure_linter/ python tools/closure_linter/closure_linter/gjslint.py --unix_mode --strict --nojsdoc -r lib/ -r src/ -r test/

cpplint:
	@python tools/cpplint.py $(wildcard src/*.cc src/*.h src/*.c)

lint: jslint cpplint

.PHONY: lint cpplint jslint bench clean docopen docclean doc dist distclean check uninstall install all program staticlib dynamiclib test test-all website-upload
