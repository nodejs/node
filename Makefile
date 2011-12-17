BUILDTYPE ?= Release

ifeq ($(BUILDTYPE),Release)
all: out/Makefile node
else
all: out/Makefile node_g
endif

node: out/Release/node
	-ln -fs out/Release/node node

out/Release/node:
	$(MAKE) -C out BUILDTYPE=Release

node_g: out/Debug/node
	-ln -fs out/Debug/node node_g

out/Debug/node:
	$(MAKE) -C out BUILDTYPE=Debug

out/Makefile: common.gypi deps/uv/uv.gyp deps/http_parser/http_parser.gyp deps/zlib/zlib.gyp deps/v8/build/common.gypi deps/v8/tools/gyp/v8.gyp node.gyp options.gypi
	tools/gyp_node -f make

install: all
	out/Release/node tools/installer.js ./options.gypi install

uninstall:
	out/Release/node tools/installer.js ./options.gypi uninstall

clean:
	-rm -rf out/Makefile node node_g out/**/*.o  out/**/*.a out/$(BUILDTYPE)/node

distclean:
	-rm -rf out
	-rm options.gypi

test: all
	python tools/test.py --mode=release simple message

test-http1: all
	python tools/test.py --mode=release --use-http1 simple message

test-valgrind: all
	python tools/test.py --mode=release --valgrind simple message

test-all: all
	python tools/test.py --mode=debug,release

test-all-http1: all
	python tools/test.py --mode=debug,release --use-http1

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

apidoc_sources = $(wildcard doc/api/*.markdown)
apidocs = $(addprefix out/,$(apidoc_sources:.markdown=.html))

apidoc_dirs = out/doc out/doc/api/ out/doc/api/assets out/doc/about out/doc/community out/doc/logos

apiassets = $(subst api_assets,api/assets,$(addprefix out/,$(wildcard doc/api_assets/*)))

website_files = \
	out/doc/index.html    \
	out/doc/v0.4_announcement.html   \
	out/doc/cla.html      \
	out/doc/sh_main.js    \
	out/doc/sh_javascript.min.js \
	out/doc/sh_vim-dark.css \
	out/doc/logo.png      \
	out/doc/sponsored.png \
	out/doc/favicon.ico   \
	out/doc/pipe.css \
	out/doc/about/index.html \
	out/doc/close-downloads.png \
	out/doc/community/index.html \
	out/doc/community/not-invented-here.png \
	out/doc/download-logo.png \
	out/doc/ebay-logo.png \
	out/doc/footer-logo.png \
	out/doc/icons.png \
	out/doc/linkedin-logo.png \
	out/doc/logos/index.html \
	out/doc/microsoft-logo.png \
	out/doc/platform-icons.png \
	out/doc/ryan-speaker.jpg \
	out/doc/yahoo-logo.png

doc: out/Release/node $(apidoc_dirs) $(website_files) $(apiassets) $(apidocs)

$(apidoc_dirs):
	mkdir -p $@

out/doc/api/assets/%: doc/api_assets/% out/doc/api/assets/
	cp $< $@

out/doc/%: doc/%
	cp $< $@

out/doc/api/%.html: doc/api/%.markdown out/Release/node $(apidoc_dirs) $(apiassets) tools/doctool/doctool.js
	out/Release/node tools/doctool/doctool.js doc/template.html $< > $@

out/doc/%:

website-upload: doc
	rsync -r out/doc/ node@nodejs.org:~/web/nodejs.org/

docopen: out/doc/api/all.html
	-google-chrome out/doc/api/all.html

docclean:
	-rm -rf out/doc

VERSION=$(shell git describe)
TARNAME=node-$(VERSION)

#dist: doc/node.1 doc/api
dist: doc
	git archive --format=tar --prefix=$(TARNAME)/ HEAD | tar xf -
	mkdir -p $(TARNAME)/doc
	cp doc/node.1 $(TARNAME)/doc/node.1
	cp -r out/doc/api $(TARNAME)/doc/api
	rm -rf $(TARNAME)/deps/v8/test # too big
	rm -rf $(TARNAME)/doc/logos # too big
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

.PHONY: lint cpplint jslint bench clean docopen docclean doc dist distclean check uninstall install install-includes install-bin all program staticlib dynamiclib test test-all website-upload
