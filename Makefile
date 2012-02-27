PYTHON ?= python
WAF    = $(PYTHON) tools/waf-light

web_root = node@nodejs.org:~/web/nodejs.org/

#
# Because we recursively call make from waf we need to make sure that we are
# using the correct make. Not all makes are GNU Make, but this likely only
# works with gnu make. To deal with this we remember how the user invoked us
# via a make builtin variable and use that in all subsequent operations
#
export NODE_MAKE := $(MAKE)

all: program
	@-[ -f out/Release/node ] && ls -lh out/Release/node

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
	$(PYTHON) tools/test.py --mode=release simple message

test-http1: all
	$(PYTHON) tools/test.py --mode=release --use-http1 simple message

test-valgrind: all
	$(PYTHON) tools/test.py --mode=release --valgrind simple message

test-all: all
	$(PYTHON) tools/test.py --mode=debug,release
	make test-npm

test-all-http1: all
	$(PYTHON) tools/test.py --mode=debug,release --use-http1

test-all-valgrind: all
	$(PYTHON) tools/test.py --mode=debug,release --valgrind

test-release: all
	$(PYTHON) tools/test.py --mode=release

test-debug: all
	$(PYTHON) tools/test.py --mode=debug

test-message: all
	$(PYTHON) tools/test.py message

test-simple: all
	$(PYTHON) tools/test.py simple

test-pummel: all
	$(PYTHON) tools/test.py pummel

test-internet: all
	$(PYTHON) tools/test.py internet

test-npm: all
	./node deps/npm/test/run.js

test-npm-publish: all
	npm_package_config_publishtest=true ./node deps/npm/test/run.js

out/Release/node: all

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
	out/doc/sh.css \
	out/doc/logo.png      \
	out/doc/favicon.ico   \
	out/doc/pipe.css \
	out/doc/about/index.html \
	out/doc/close-downloads.png \
	out/doc/community/index.html \
	out/doc/community/not-invented-here.png \
	out/doc/logos/index.html \
	out/doc/microsoft-logo.png \
	out/doc/ryan-speaker.jpg \
	out/doc/download-logo.png \
	out/doc/ebay-logo.png \
	out/doc/footer-logo-alt.png \
	out/doc/footer-logo.png \
	out/doc/icons-interior.png \
	out/doc/icons.png \
	out/doc/home-icons.png \
	out/doc/joyent-logo_orange_nodeorg-01.png \
	out/doc/linkedin-logo.png \
	out/doc/logo-light.png \
	out/doc/mac_osx_nodejs_installer_logo.png \
	out/doc/microsoft-logo.png \
	out/doc/platform-icons.png \
	out/doc/sponsored.png \
	out/doc/twitter-bird.png \
	out/doc/community-icons.png \
	out/doc/yahoo-logo.png

doc docs: out/Release/node $(apidoc_dirs) $(website_files) $(apiassets) $(apidocs)

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

clean:
	$(WAF) clean
	-find tools -name "*.pyc" | xargs rm -f

distclean: docclean
	-find tools -name "*.pyc" | xargs rm -f
	-rm -rf dist-osx
	-rm -rf out/ node node_g

check:
	@tools/waf-light check

VERSION=v$(shell $(PYTHON) tools/getnodeversion.py)
TARNAME=node-$(VERSION)
TARBALL=$(TARNAME).tar.gz
PKG=out/$(TARNAME).pkg

packagemaker=/Developer/Applications/Utilities/PackageMaker.app/Contents/MacOS/PackageMaker

#dist: doc/node.1 doc/api
dist: $(TARBALL) $(PKG)

PKGDIR=out/dist-osx

pkg: $(PKG)

$(PKG):
	-rm -rf $(PKGDIR)
	# Need to remove deps between architecture changes.
	rm -rf out/*/deps
	$(WAF) configure --prefix=/usr/local --without-snapshot --dest-cpu=ia32
	CFLAGS=-m32 DESTDIR=$(PKGDIR)/32 $(WAF) install
	rm -rf out/*/deps
	$(WAF) configure --prefix=/usr/local --without-snapshot --dest-cpu=x64
	CFLAGS=-m64 DESTDIR=$(PKGDIR) $(WAF) install
	lipo $(PKGDIR)/32/usr/local/bin/node \
		$(PKGDIR)/usr/local/bin/node \
		-output $(PKGDIR)/usr/local/bin/node-universal \
		-create
	mv $(PKGDIR)/usr/local/bin/node-universal $(PKGDIR)/usr/local/bin/node
	rm -rf $(PKGDIR)/32
	$(packagemaker) \
		--id "org.nodejs.NodeJS-$(VERSION)" \
		--doc tools/osx-pkg.pmdoc \
		--out $(PKG)

$(TARBALL): out/doc
	git archive --format=tar --prefix=$(TARNAME)/ HEAD | tar xf -
	mkdir -p $(TARNAME)/doc
	cp doc/node.1 $(TARNAME)/doc/node.1
	cp -r out/doc/api $(TARNAME)/doc/api
	rm -rf $(TARNAME)/deps/v8/test # too big
	rm -rf $(TARNAME)/doc/logos/*.{png,eps} # too big
	tar -cf $(TARNAME).tar $(TARNAME)
	rm -rf $(TARNAME)
	gzip -f -9 $(TARNAME).tar

dist-upload: $(TARBALL) $(PKG)
	ssh node@nodejs.org mkdir -p web/nodejs.org/dist/$(VERSION)
	scp $(TARBALL) node@nodejs.org:~/web/nodejs.org/dist/$(VERSION)/$(TARBALL)
	scp $(PKG) node@nodejs.org:~/web/nodejs.org/dist/$(VERSION)/$(TARNAME).pkg

bench:
	 benchmark/http_simple_bench.sh

bench-idle:
	./node benchmark/idle_server.js &
	sleep 1
	./node benchmark/idle_clients.js &

jslint:
	PYTHONPATH=tools/closure_linter/ $(PYTHON) tools/closure_linter/closure_linter/gjslint.py --unix_mode --strict --nojsdoc -r lib/ -r src/ -r test/ --exclude_files lib/punycode.js

cpplint:
	@$(PYTHON) tools/cpplint.py $(wildcard src/*.cc src/*.h src/*.c)

lint: jslint cpplint

.PHONY: lint cpplint jslint bench clean docopen docclean doc dist distclean dist-upload check uninstall install all program staticlib dynamiclib test test-all website-upload
