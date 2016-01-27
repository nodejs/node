-include config.mk

BUILDTYPE ?= Release
PYTHON ?= python
NINJA ?= ninja
DESTDIR ?=
SIGN ?=
FLAKY_TESTS ?= run
STAGINGSERVER ?= node-www

OSTYPE := $(shell uname -s | tr '[A-Z]' '[a-z]')

# Flags for packaging.
NODE ?= ./node

# Default to verbose builds.
# To do quiet/pretty builds, run `make V=` to set V to an empty string,
# or set the V environment variable to an empty string.
V ?= 1

# BUILDTYPE=Debug builds both release and debug builds. If you want to compile
# just the debug build, run `make -C out BUILDTYPE=Debug` instead.
ifeq ($(BUILDTYPE),Release)
all: out/Makefile node
else
all: out/Makefile node node_g
endif

# The .PHONY is needed to ensure that we recursively use the out/Makefile
# to check for changes.
.PHONY: node node_g

ifeq ($(USE_NINJA),1)
node: config.gypi
	$(NINJA) -C out/Release/
	ln -fs out/Release/node node

node_g: config.gypi
	$(NINJA) -C out/Debug/
	ln -fs out/Debug/node $@
else
node: config.gypi out/Makefile
	$(MAKE) -C out BUILDTYPE=Release V=$(V)
	ln -fs out/Release/node node

node_g: config.gypi out/Makefile
	$(MAKE) -C out BUILDTYPE=Debug V=$(V)
	ln -fs out/Debug/node $@
endif

out/Makefile: common.gypi deps/uv/uv.gyp deps/http_parser/http_parser.gyp deps/zlib/zlib.gyp deps/v8/build/common.gypi deps/v8/tools/gyp/v8.gyp node.gyp config.gypi
ifeq ($(USE_NINJA),1)
	touch out/Makefile
	$(PYTHON) tools/gyp_node.py -f ninja
else
	$(PYTHON) tools/gyp_node.py -f make
endif

config.gypi: configure
	$(PYTHON) ./configure

install: all
	$(PYTHON) tools/install.py $@ $(DESTDIR)

uninstall:
	$(PYTHON) tools/install.py $@ $(DESTDIR)

clean:
	-rm -rf out/Makefile node node_g out/$(BUILDTYPE)/node blog.html email.md
	-find out/ -name '*.o' -o -name '*.a' | xargs rm -rf
	-rm -rf node_modules

distclean:
	-rm -rf out
	-rm -f config.gypi
	-rm -f config.mk
	-rm -rf node node_g blog.html email.md
	-rm -rf node_modules

test: all
	$(PYTHON) tools/test.py --mode=release simple message
	$(MAKE) jslint

test-http1: all
	$(PYTHON) tools/test.py --mode=release --use-http1 simple message

test-valgrind: all
	$(PYTHON) tools/test.py --mode=release --valgrind simple message

test/gc/node_modules/weak/build/Release/weakref.node:
	@if [ ! -f node ]; then make all; fi
	./node deps/npm/node_modules/node-gyp/bin/node-gyp rebuild \
		--directory="$(shell pwd)/test/gc/node_modules/weak" \
		--nodedir="$(shell pwd)"

test-gc: all test/gc/node_modules/weak/build/Release/weakref.node
	$(PYTHON) tools/test.py --mode=release gc

test-all: all test/gc/node_modules/weak/build/Release/weakref.node
	$(PYTHON) tools/test.py --mode=debug,release
	make test-npm

test-all-http1: all
	$(PYTHON) tools/test.py --mode=debug,release --use-http1

test-all-valgrind: all
	$(PYTHON) tools/test.py --mode=debug,release --valgrind

test-ci:
	$(PYTHON) tools/test.py -p tap --logfile test.tap --mode=release --arch=$(DESTCPU) --flaky-tests=$(FLAKY_TESTS) simple message internet

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

test-npm: node
	./node deps/npm/test/run.js

test-npm-publish: node
	npm_package_config_publishtest=true ./node deps/npm/test/run.js

test-timers:
	$(MAKE) --directory=tools faketime
	$(PYTHON) tools/test.py --mode=release timers

test-timers-clean:
	$(MAKE) --directory=tools clean

apidoc_sources = $(wildcard doc/api/*.markdown)
apidocs = $(addprefix out/,$(apidoc_sources:.markdown=.html)) \
          $(addprefix out/,$(apidoc_sources:.markdown=.json))

apidoc_dirs = out/doc out/doc/api/ out/doc/api/assets

apiassets = $(subst api_assets,api/assets,$(addprefix out/,$(wildcard doc/api_assets/*)))

website_files = \
	out/doc/sh_main.js    \
	out/doc/sh_javascript.min.js

doc: $(apidoc_dirs) $(website_files) $(apiassets) $(apidocs) tools/doc/ out/doc/changelog.html node

doc-branch: NODE_DOC_VERSION = v$(shell $(PYTHON) tools/getnodeversion.py | cut -f1,2 -d.)
doc-branch: doc

$(apidoc_dirs):
	mkdir -p $@

out/doc/api/assets/%: doc/api_assets/% out/doc/api/assets/
	cp $< $@

out/doc/changelog.html: ChangeLog doc/changelog-head.html doc/changelog-foot.html tools/build-changelog.sh node
	bash tools/build-changelog.sh

out/doc/%: doc/%
	cp -r $< $@

out/doc/api/%.json: doc/api/%.markdown node
	NODE_DOC_VERSION=$(NODE_DOC_VERSION) out/Release/node tools/doc/generate.js --format=json $< > $@

out/doc/api/%.html: doc/api/%.markdown node
	NODE_DOC_VERSION=$(NODE_DOC_VERSION) out/Release/node tools/doc/generate.js --format=html --template=doc/template.html $< > $@

email.md: ChangeLog tools/email-footer.md
	bash tools/changelog-head.sh | sed 's|^\* #|* \\#|g' > $@
	cat tools/email-footer.md | sed -e 's|__VERSION__|'$(VERSION)'|g' >> $@

blog.html: email.md
	cat $< | ./node tools/doc/node_modules/.bin/marked > $@

website-upload: doc
	rsync -r out/doc/ node@nodejs.org:~/web/nodejs.org/
	ssh node@nodejs.org '\
    rm -f ~/web/nodejs.org/dist/latest &&\
    ln -s $(VERSION) ~/web/nodejs.org/dist/latest &&\
    rm -f ~/web/nodejs.org/docs/latest &&\
    ln -s $(VERSION) ~/web/nodejs.org/docs/latest &&\
    rm -f ~/web/nodejs.org/dist/node-latest.tar.gz &&\
    ln -s $(VERSION)/node-$(VERSION).tar.gz ~/web/nodejs.org/dist/node-latest.tar.gz'

doc-branch-upload: NODE_DOC_VERSION = v$(shell $(PYTHON) tools/getnodeversion.py | cut -f1,2 -d.)
doc-branch-upload: doc-branch
	echo $(NODE_DOC_VERSION)
	rsync -r out/doc/api/ node@nodejs.org:~/web/nodejs.org/$(NODE_DOC_VERSION)

docopen: out/doc/api/all.html
	-google-chrome out/doc/api/all.html

docclean:
	-rm -rf out/doc

run-ci:
	$(PYTHON) ./configure --without-snapshot $(CONFIG_FLAGS)
	$(MAKE)
	$(MAKE) test-ci

RAWVER=$(shell $(PYTHON) tools/getnodeversion.py)
VERSION=v$(RAWVER)

# For nightly builds, you must set DISTTYPE to "nightly", "next-nightly" or
# "custom". For the nightly and next-nightly case, you need to set DATESTRING
# and COMMIT in order to properly name the build.
# For the rc case you need to set CUSTOMTAG to an appropriate CUSTOMTAG number

ifndef DISTTYPE
DISTTYPE=release
endif
ifeq ($(DISTTYPE),release)
FULLVERSION=$(VERSION)
else # ifeq ($(DISTTYPE),release)
ifeq ($(DISTTYPE),custom)
ifndef CUSTOMTAG
$(error CUSTOMTAG is not set for DISTTYPE=custom)
endif # ifndef CUSTOMTAG
TAG=$(CUSTOMTAG)
else # ifeq ($(DISTTYPE),custom)
ifndef DATESTRING
$(error DATESTRING is not set for nightly)
endif # ifndef DATESTRING
ifndef COMMIT
$(error COMMIT is not set for nightly)
endif # ifndef COMMIT
ifneq ($(DISTTYPE),nightly)
ifneq ($(DISTTYPE),next-nightly)
$(error DISTTYPE is not release, custom, nightly or next-nightly)
endif # ifneq ($(DISTTYPE),next-nightly)
endif # ifneq ($(DISTTYPE),nightly)
TAG=$(DISTTYPE)$(DATESTRING)$(COMMIT)
endif # ifeq ($(DISTTYPE),custom)
FULLVERSION=$(VERSION)-$(TAG)
endif # ifeq ($(DISTTYPE),release)

DISTTYPEDIR ?= $(DISTTYPE)
RELEASE=$(shell sed -ne 's/\#define NODE_VERSION_IS_RELEASE \([01]\)/\1/p' src/node_version.h)
NODE_DOC_VERSION=$(VERSION)
NPMVERSION=v$(shell cat deps/npm/package.json | grep '"version"' | sed 's/^[^:]*: "\([^"]*\)",.*/\1/')

ifeq ($(findstring x86_64,$(shell uname -m)),x86_64)
DESTCPU ?= x64
else
DESTCPU ?= ia32
endif
ifeq ($(DESTCPU),x64)
ARCH=x64
else
ifeq ($(DESTCPU),arm)
ARCH=arm
else
ARCH=x86
endif
endif

# enforce "x86" over "ia32" as the generally accepted way of referring to 32-bit intel
ifeq ($(ARCH),ia32)
override ARCH=x86
endif
ifeq ($(DESTCPU),ia32)
override DESTCPU=x86
endif

TARNAME=node-$(FULLVERSION)
TARBALL=$(TARNAME).tar
BINARYNAME=$(TARNAME)-$(OSTYPE)-$(ARCH)
BINARYTAR=$(BINARYNAME).tar
# OSX doesn't have xz installed by default, http://macpkg.sourceforge.net/
XZ=$(shell which xz > /dev/null 2>&1; echo $$?)
XZ_COMPRESSION ?= 9
PKG=$(TARNAME).pkg
PACKAGEMAKER ?= /Developer/Applications/Utilities/PackageMaker.app/Contents/MacOS/PackageMaker
PKGDIR=out/dist-osx

release-only:
	@if [ "$(shell git status --porcelain | egrep -v '^\?\? ')" = "" ]; then \
		exit 0 ; \
	else \
		echo "" >&2 ; \
		echo "The git repository is not clean." >&2 ; \
		echo "Please commit changes before building release tarball." >&2 ; \
		echo "" >&2 ; \
		git status --porcelain | egrep -v '^\?\?' >&2 ; \
		echo "" >&2 ; \
		exit 1 ; \
	fi
	@if [ "$(DISTTYPE)" != "release" -o "$(RELEASE)" = "1" ]; then \
		exit 0; \
	else \
		echo "" >&2 ; \
		echo "#NODE_VERSION_IS_RELEASE is set to $(RELEASE)." >&2 ; \
		echo "Did you remember to update src/node_version.h?" >&2 ; \
		echo "" >&2 ; \
		exit 1 ; \
	fi

$(PKG): release-only
	rm -rf $(PKGDIR)
	rm -rf out/deps out/Release
	$(PYTHON) ./configure \
		--dest-cpu=ia32 \
		--tag=$(TAG) \
		--without-snapshot \
		$(CONFIG_FLAGS)
	$(MAKE) install V=$(V) DESTDIR=$(PKGDIR)/32
	rm -rf out/deps out/Release
	$(PYTHON) ./configure \
		--dest-cpu=x64 \
		--tag=$(TAG) \
		--without-snapshot \
		$(CONFIG_FLAGS)
	$(MAKE) install V=$(V) DESTDIR=$(PKGDIR)
	SIGN="$(CODESIGN_CERT)" PKGDIR="$(PKGDIR)" bash tools/osx-codesign.sh
	lipo $(PKGDIR)/32/usr/local/bin/node \
		$(PKGDIR)/usr/local/bin/node \
		-output $(PKGDIR)/usr/local/bin/node-universal \
		-create
	mv $(PKGDIR)/usr/local/bin/node-universal $(PKGDIR)/usr/local/bin/node
	rm -rf $(PKGDIR)/32
	$(PACKAGEMAKER) \
		--id "org.nodejs.Node" \
		--doc tools/osx-pkg.pmdoc \
		--out $(PKG)
	SIGN="$(PRODUCTSIGN_CERT)" PKG="$(PKG)" bash tools/osx-productsign.sh

pkg: $(PKG)

pkg-upload: pkg
	ssh $(STAGINGSERVER) "mkdir -p nodejs/$(DISTTYPEDIR)/$(FULLVERSION)"
	chmod 664 node-$(FULLVERSION).pkg
	scp -p node-$(FULLVERSION).pkg $(STAGINGSERVER):nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/node-$(FULLVERSION).pkg
	ssh $(STAGINGSERVER) "touch nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/node-$(FULLVERSION).pkg.done"

$(TARBALL): release-only node doc
	git archive --format=tar --prefix=$(TARNAME)/ HEAD | tar xf -
	mkdir -p $(TARNAME)/doc/api
	cp doc/node.1 $(TARNAME)/doc/node.1
	cp -r out/doc/api/* $(TARNAME)/doc/api/
	rm -rf $(TARNAME)/deps/v8/test # too big
	rm -rf $(TARNAME)/doc/images # too big
	find $(TARNAME)/ -type l | xargs rm # annoying on windows
	tar -cf $(TARNAME).tar $(TARNAME)
	rm -rf $(TARNAME)
	gzip -c -f -9 $(TARNAME).tar > $(TARNAME).tar.gz
ifeq ($(XZ), 0)
	xz -c -f -$(XZ_COMPRESSION) $(TARNAME).tar > $(TARNAME).tar.xz
endif
	rm $(TARNAME).tar

tar: $(TARBALL)

tar-upload: tar
	ssh $(STAGINGSERVER) "mkdir -p nodejs/$(DISTTYPEDIR)/$(FULLVERSION)"
	chmod 664 node-$(FULLVERSION).tar.gz
	scp -p node-$(FULLVERSION).tar.gz $(STAGINGSERVER):nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/node-$(FULLVERSION).tar.gz
	ssh $(STAGINGSERVER) "touch nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/node-$(FULLVERSION).tar.gz.done"
ifeq ($(XZ), 0)
	chmod 664 node-$(FULLVERSION).tar.xz
	scp -p node-$(FULLVERSION).tar.xz $(STAGINGSERVER):nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/node-$(FULLVERSION).tar.xz
	ssh $(STAGINGSERVER) "touch nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/node-$(FULLVERSION).tar.xz.done"
endif

doc-upload: tar
	ssh $(STAGINGSERVER) "mkdir -p nodejs/$(DISTTYPEDIR)/$(FULLVERSION)"
	chmod -R ug=rw-x+X,o=r+X out/doc/
	scp -pr out/doc/ $(STAGINGSERVER):nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/docs/
	ssh $(STAGINGSERVER) "touch nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/docs.done"

$(TARBALL)-headers: config.gypi release-only
	$(PYTHON) ./configure \
		--prefix=/ \
		--dest-cpu=$(DESTCPU) \
		--tag=$(TAG) \
		$(CONFIG_FLAGS)
	HEADERS_ONLY=1 $(PYTHON) tools/install.py install '$(TARNAME)' '/'
	find $(TARNAME)/ -type l | xargs rm # annoying on windows
	tar -cf $(TARNAME)-headers.tar $(TARNAME)
	rm -rf $(TARNAME)
	gzip -c -f -9 $(TARNAME)-headers.tar > $(TARNAME)-headers.tar.gz
ifeq ($(XZ), 0)
	xz -c -f -$(XZ_COMPRESSION) $(TARNAME)-headers.tar > $(TARNAME)-headers.tar.xz
endif
	rm $(TARNAME)-headers.tar

tar-headers: $(TARBALL)-headers

tar-headers-upload: tar-headers
	ssh $(STAGINGSERVER) "mkdir -p nodejs/$(DISTTYPEDIR)/$(FULLVERSION)"
	chmod 664 $(TARNAME)-headers.tar.gz
	scp -p $(TARNAME)-headers.tar.gz $(STAGINGSERVER):nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/$(TARNAME)-headers.tar.gz
	ssh $(STAGINGSERVER) "touch nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/$(TARNAME)-headers.tar.gz.done"
ifeq ($(XZ), 0)
	chmod 664 $(TARNAME)-headers.tar.xz
	scp -p $(TARNAME)-headers.tar.xz $(STAGINGSERVER):nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/$(TARNAME)-headers.tar.xz
	ssh $(STAGINGSERVER) "touch nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/$(TARNAME)-headers.tar.xz.done"
endif

$(BINARYTAR): release-only
	rm -rf $(BINARYNAME)
	rm -rf out/deps out/Release
	$(PYTHON) ./configure --prefix=/ --without-snapshot --dest-cpu=$(DESTCPU) --tag=$(TAG) $(CONFIG_FLAGS)
	$(MAKE) install DESTDIR=$(BINARYNAME) V=$(V) PORTABLE=1
	cp README.md $(BINARYNAME)
	cp LICENSE $(BINARYNAME)
	cp ChangeLog $(BINARYNAME)
	tar -cf $(BINARYNAME).tar $(BINARYNAME)
	rm -rf $(BINARYNAME)
	gzip -c -f -9 $(BINARYNAME).tar > $(BINARYNAME).tar.gz
ifeq ($(XZ), 0)
	xz -c -f -$(XZ_COMPRESSION) $(BINARYNAME).tar > $(BINARYNAME).tar.xz
endif
	rm $(BINARYNAME).tar

binary: $(BINARYTAR)

binary-upload-arch: binary
	ssh $(STAGINGSERVER) "mkdir -p nodejs/$(DISTTYPEDIR)/$(FULLVERSION)"
	chmod 664 node-$(FULLVERSION)-$(OSTYPE)-$(ARCH).tar.gz
	scp -p node-$(FULLVERSION)-$(OSTYPE)-$(ARCH).tar.gz $(STAGINGSERVER):nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/node-$(FULLVERSION)-$(OSTYPE)-$(ARCH).tar.gz
	ssh $(STAGINGSERVER) "touch nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/node-$(FULLVERSION)-$(OSTYPE)-$(ARCH).tar.gz.done"
ifeq ($(XZ), 0)
	chmod 664 node-$(FULLVERSION)-$(OSTYPE)-$(ARCH).tar.xz
	scp -p node-$(FULLVERSION)-$(OSTYPE)-$(ARCH).tar.xz $(STAGINGSERVER):nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/node-$(FULLVERSION)-$(OSTYPE)-$(ARCH).tar.xz
	ssh $(STAGINGSERVER) "touch nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/node-$(FULLVERSION)-$(OSTYPE)-$(ARCH).tar.xz.done"
endif

ifeq ($(OSTYPE),darwin)
binary-upload:
	$(MAKE) binary-upload-arch \
		DESTCPU=ia32 \
		ARCH=x86 \
		DISTTYPE=$(DISTTYPE) \
		DATESTRING=$(DATESTRING) \
		COMMIT=$(COMMIT) \
		CUSTOMTAG=$(CUSTOMTAG) \
		CONFIG_FLAGS=$(CONFIG_FLAGS)
	$(MAKE) binary-upload-arch \
		DESTCPU=x64 \
		ARCH=x64 \
		DISTTYPE=$(DISTTYPE) \
		DATESTRING=$(DATESTRING) \
		COMMIT=$(COMMIT) \
		CUSTOMTAG=$(CUSTOMTAG) \
		CONFIG_FLAGS=$(CONFIG_FLAGS)
else
binary-upload: binary-upload-arch
endif


$(PKGSRC): release-only
	rm -rf dist out
	$(PYTHON) configure --prefix=/ --without-snapshot \
		--dest-cpu=$(DESTCPU) --tag=$(TAG) $(CONFIG_FLAGS)
	$(MAKE) install DESTDIR=dist
	(cd dist; find * -type f | sort) > packlist
	pkg_info -X pkg_install | \
		egrep '^(MACHINE_ARCH|OPSYS|OS_VERSION|PKGTOOLS_VERSION)' > build-info
	pkg_create -B build-info -c tools/pkgsrc/comment -d tools/pkgsrc/description \
		-f packlist -I /opt/local -p dist -U $(PKGSRC)

pkgsrc: $(PKGSRC)

wrkclean:
	$(MAKE) -C tools/wrk/ clean
	rm tools/wrk/wrk

wrk: tools/wrk/wrk
tools/wrk/wrk:
	$(MAKE) -C tools/wrk/

bench-net: all
	@$(NODE) benchmark/common.js net

bench-crypto: all
	@$(NODE) benchmark/common.js crypto

bench-tls: all
	@$(NODE) benchmark/common.js tls

bench-http: wrk all
	@$(NODE) benchmark/common.js http

bench-fs: all
	@$(NODE) benchmark/common.js fs

bench-misc: all
	@$(MAKE) -C benchmark/misc/function_call/
	@$(NODE) benchmark/common.js misc

bench-array: all
	@$(NODE) benchmark/common.js arrays

bench-buffer: all
	@$(NODE) benchmark/common.js buffers

bench-all: bench bench-misc bench-array bench-buffer

bench: bench-net bench-http bench-fs bench-tls

bench-http-simple:
	 benchmark/http_simple_bench.sh

bench-idle:
	./node benchmark/idle_server.js &
	sleep 1
	./node benchmark/idle_clients.js &

jslintfix:
	PYTHONPATH=tools/closure_linter/ $(PYTHON) tools/closure_linter/closure_linter/fixjsstyle.py --strict --nojsdoc -r lib/ -r src/ --exclude_files lib/punycode.js

jslint:
	PYTHONPATH=tools/closure_linter/ $(PYTHON) tools/closure_linter/closure_linter/gjslint.py --unix_mode --strict --nojsdoc -r lib/ -r src/ --exclude_files lib/punycode.js

cpplint:
	@$(PYTHON) tools/cpplint.py $(wildcard src/*.cc src/*.h src/*.c)

lint: jslint cpplint

.PHONY: lint cpplint jslint bench clean docopen docclean doc dist distclean \
	check uninstall install install-includes install-bin all staticlib \
	dynamiclib test test-all website-upload pkg blog blogclean tar binary \
	release-only bench-http-simple bench-idle bench-all bench bench-misc \
	bench-array bench-buffer bench-net bench-http bench-fs bench-tls run-ci
