-include config.mk

BUILDTYPE ?= Release
PYTHON ?= python
DESTDIR ?=
SIGN ?=
PREFIX ?= /usr/local
FLAKY_TESTS ?= run
TEST_CI_ARGS ?=
STAGINGSERVER ?= node-www
LOGLEVEL ?= silent
OSTYPE := $(shell uname -s | tr '[A-Z]' '[a-z]')

ifdef JOBS
  PARALLEL_ARGS = -j $(JOBS)
endif

ifdef QUICKCHECK
  QUICKCHECK_ARG := --quickcheck
endif

ifdef ENABLE_V8_TAP
  TAP_V8 := --junitout $(PWD)/v8-tap.xml
  TAP_V8_INTL := --junitout $(PWD)/v8-intl-tap.xml
  TAP_V8_BENCHMARKS := --junitout $(PWD)/v8-benchmarks-tap.xml
endif

V8_TEST_OPTIONS = $(V8_EXTRA_TEST_OPTIONS)
ifdef DISABLE_V8_I18N
  V8_TEST_OPTIONS += --noi18n
  V8_BUILD_OPTIONS += i18nsupport=off
endif

BUILDTYPE_LOWER := $(shell echo $(BUILDTYPE) | tr '[A-Z]' '[a-z]')

# Determine EXEEXT
EXEEXT := $(shell $(PYTHON) -c \
		"import sys; print('.exe' if sys.platform == 'win32' else '')")

NODE_EXE = node$(EXEEXT)
NODE ?= ./$(NODE_EXE)
NODE_G_EXE = node_g$(EXEEXT)
NPM ?= ./deps/npm/bin/npm-cli.js

# Flags for packaging.
BUILD_DOWNLOAD_FLAGS ?= --download=all
BUILD_INTL_FLAGS ?= --with-intl=small-icu
BUILD_RELEASE_FLAGS ?= $(BUILD_DOWNLOAD_FLAGS) $(BUILD_INTL_FLAGS)

# Default to verbose builds.
# To do quiet/pretty builds, run `make V=` to set V to an empty string,
# or set the V environment variable to an empty string.
V ?= 1

# BUILDTYPE=Debug builds both release and debug builds. If you want to compile
# just the debug build, run `make -C out BUILDTYPE=Debug` instead.
ifeq ($(BUILDTYPE),Release)
all: out/Makefile $(NODE_EXE)
else
all: out/Makefile $(NODE_EXE) $(NODE_G_EXE)
endif

# The .PHONY is needed to ensure that we recursively use the out/Makefile
# to check for changes.
.PHONY: $(NODE_EXE) $(NODE_G_EXE)

# The -r/-L check stops it recreating the link if it is already in place,
# otherwise $(NODE_EXE) being a .PHONY target means it is always re-run.
# Without the check there is a race condition between the link being deleted
# and recreated which can break the addons build when running test-ci
# See comments on the build-addons target for some more info
$(NODE_EXE): config.gypi out/Makefile
	$(MAKE) -C out BUILDTYPE=Release V=$(V)
	if [ ! -r $@ -o ! -L $@ ]; then ln -fs out/Release/$(NODE_EXE) $@; fi

$(NODE_G_EXE): config.gypi out/Makefile
	$(MAKE) -C out BUILDTYPE=Debug V=$(V)
	if [ ! -r $@ -o ! -L $@ ]; then ln -fs out/Debug/$(NODE_EXE) $@; fi

out/Makefile: common.gypi deps/uv/uv.gyp deps/http_parser/http_parser.gyp deps/zlib/zlib.gyp deps/v8/build/toolchain.gypi deps/v8/build/features.gypi deps/v8/tools/gyp/v8.gyp node.gyp config.gypi
	$(PYTHON) tools/gyp_node.py -f make

config.gypi: configure
	$(error Missing or stale $@, please run ./$<)

install: all
	$(PYTHON) tools/install.py $@ '$(DESTDIR)' '$(PREFIX)'

uninstall:
	$(PYTHON) tools/install.py $@ '$(DESTDIR)' '$(PREFIX)'

clean:
	$(RM) -r out/Makefile $(NODE_EXE) $(NODE_G_EXE) out/$(BUILDTYPE)/$(NODE_EXE) \
                out/$(BUILDTYPE)/node.exp
	@if [ -d out ]; then find out/ -name '*.o' -o -name '*.a' -o -name '*.d' | xargs $(RM) -r; fi
	$(RM) -r node_modules
	@if [ -d deps/icu ]; then echo deleting deps/icu; $(RM) -r deps/icu; fi
	$(RM) test.tap

distclean:
	$(RM) -r out
	$(RM) config.gypi icu_config.gypi config_fips.gypi
	$(RM) config.mk
	$(RM) -r $(NODE_EXE) $(NODE_G_EXE)
	$(RM) -r node_modules
	$(RM) -r deps/icu
	$(RM) -r deps/icu4c*.tgz deps/icu4c*.zip deps/icu-tmp
	$(RM) $(BINARYTAR).* $(TARBALL).*
	$(RM) -r deps/v8/testing/gmock
	$(RM) -r deps/v8/testing/gtest

check: test

cctest: all
	@out/$(BUILDTYPE)/$@

v8:
	tools/make-v8.sh
	$(MAKE) -C deps/v8 $(V8_ARCH).$(BUILDTYPE_LOWER) $(V8_BUILD_OPTIONS)

test: all
	$(MAKE) build-addons
	$(MAKE) build-addons-napi
	$(MAKE) cctest
	$(PYTHON) tools/test.py --mode=release -J \
		doctool inspector known_issues message pseudo-tty parallel sequential addons addons-napi
	$(MAKE) lint

test-parallel: all
	$(PYTHON) tools/test.py --mode=release parallel -J

test-valgrind: all
	$(PYTHON) tools/test.py --mode=release --valgrind sequential parallel message

test/gc/node_modules/weak/build/Release/weakref.node: $(NODE_EXE)
	$(NODE) deps/npm/node_modules/node-gyp/bin/node-gyp rebuild \
		--python="$(PYTHON)" \
		--directory="$(shell pwd)/test/gc/node_modules/weak" \
		--nodedir="$(shell pwd)"

# Implicitly depends on $(NODE_EXE), see the build-addons rule for rationale.
DOCBUILDSTAMP_PREREQS = tools/doc/addon-verify.js doc/api/addons.md

ifeq ($(OSTYPE),aix)
DOCBUILDSTAMP_PREREQS := $(DOCBUILDSTAMP_PREREQS) out/$(BUILDTYPE)/node.exp
endif

test/addons/.docbuildstamp: $(DOCBUILDSTAMP_PREREQS)
	$(RM) -r test/addons/??_*/
	$(NODE) $<
	touch $@

ADDONS_BINDING_GYPS := \
	$(filter-out test/addons/??_*/binding.gyp, \
		$(wildcard test/addons/*/binding.gyp))

ADDONS_BINDING_SOURCES := \
	$(filter-out test/addons/??_*/*.cc, $(wildcard test/addons/*/*.cc)) \
	$(filter-out test/addons/??_*/*.h, $(wildcard test/addons/*/*.h))

# Implicitly depends on $(NODE_EXE), see the build-addons rule for rationale.
# Depends on node-gyp package.json so that build-addons is (re)executed when
# node-gyp is updated as part of an npm update.
test/addons/.buildstamp: config.gypi \
	deps/npm/node_modules/node-gyp/package.json \
	$(ADDONS_BINDING_GYPS) $(ADDONS_BINDING_SOURCES) \
	deps/uv/include/*.h deps/v8/include/*.h \
	src/node.h src/node_buffer.h src/node_object_wrap.h src/node_version.h \
	test/addons/.docbuildstamp
#	Cannot use $(wildcard test/addons/*/) here, it's evaluated before
#	embedded addons have been generated from the documentation.
#	Ignore folders without binding.gyp (#14843)
	@for dirname in test/addons/*/; do \
		if [ ! -f "$$PWD/$${dirname}binding.gyp" ]; then \
			continue; fi ; \
		printf "\nBuilding addon $$PWD/$$dirname\n" ; \
		env MAKEFLAGS="-j1" $(NODE) deps/npm/node_modules/node-gyp/bin/node-gyp \
		        --loglevel=$(LOGLEVEL) rebuild \
			--python="$(PYTHON)" \
			--directory="$$PWD/$$dirname" \
			--nodedir="$$PWD" || exit 1 ; \
	done
	touch $@

# .buildstamp and .docbuildstamp need $(NODE_EXE) but cannot depend on it
# directly because it calls make recursively.  The parent make cannot know
# if the subprocess touched anything so it pessimistically assumes that
# .buildstamp and .docbuildstamp are out of date and need a rebuild.
# Just goes to show that recursive make really is harmful...
# TODO(bnoordhuis) Force rebuild after gyp update.
build-addons: $(NODE_EXE) test/addons/.buildstamp

ADDONS_NAPI_BINDING_GYPS := \
	$(filter-out test/addons-napi/??_*/binding.gyp, \
		$(wildcard test/addons-napi/*/binding.gyp))

ADDONS_NAPI_BINDING_SOURCES := \
	$(filter-out test/addons-napi/??_*/*.cc, $(wildcard test/addons-napi/*/*.cc)) \
	$(filter-out test/addons-napi/??_*/*.h, $(wildcard test/addons-napi/*/*.h))

# Implicitly depends on $(NODE_EXE), see the build-addons-napi rule for rationale.
test/addons-napi/.buildstamp: config.gypi \
	deps/npm/node_modules/node-gyp/package.json \
	$(ADDONS_NAPI_BINDING_GYPS) $(ADDONS_NAPI_BINDING_SOURCES) \
	deps/uv/include/*.h deps/v8/include/*.h \
	src/node.h src/node_buffer.h src/node_object_wrap.h src/node_version.h \
	src/node_api.h src/node_api_types.h
#	Cannot use $(wildcard test/addons-napi/*/) here, it's evaluated before
#	embedded addons have been generated from the documentation.
	@for dirname in test/addons-napi/*/; do \
		printf "\nBuilding addon $$PWD/$$dirname\n" ; \
		env MAKEFLAGS="-j1" $(NODE) deps/npm/node_modules/node-gyp/bin/node-gyp \
		        --loglevel=$(LOGLEVEL) rebuild \
			--python="$(PYTHON)" \
			--directory="$$PWD/$$dirname" \
			--nodedir="$$PWD" || exit 1 ; \
	done
	touch $@

# .buildstamp and .docbuildstamp need $(NODE_EXE) but cannot depend on it
# directly because it calls make recursively.  The parent make cannot know
# if the subprocess touched anything so it pessimistically assumes that
# .buildstamp and .docbuildstamp are out of date and need a rebuild.
# Just goes to show that recursive make really is harmful...
# TODO(bnoordhuis) Force rebuild after gyp or node-gyp update.
build-addons-napi: $(NODE_EXE) test/addons-napi/.buildstamp

clear-stalled:
	# Clean up any leftover processes but don't error if found.
	ps awwx | grep Release/node | grep -v grep | cat
	@PS_OUT=`ps awwx | grep Release/node | grep -v grep | awk '{print $$1}'`; \
	if [ "$${PS_OUT}" ]; then \
		echo $${PS_OUT} | xargs kill; \
	fi

test-gc: all test/gc/node_modules/weak/build/Release/weakref.node
	$(PYTHON) tools/test.py --mode=release gc

test-build: | all build-addons build-addons-napi

test-build-addons-napi: all build-addons-napi

test-all: test-build test/gc/node_modules/weak/build/Release/weakref.node
	$(PYTHON) tools/test.py --mode=debug,release

test-all-valgrind: test-build
	$(PYTHON) tools/test.py --mode=debug,release --valgrind

CI_NATIVE_SUITES := addons addons-napi
CI_JS_SUITES := doctool inspector known_issues message parallel pseudo-tty sequential

# Build and test addons without building anything else
test-ci-native: LOGLEVEL := info
test-ci-native: | test/addons/.buildstamp test/addons-napi/.buildstamp
	$(PYTHON) tools/test.py $(PARALLEL_ARGS) -p tap --logfile test.tap \
		--mode=release --flaky-tests=$(FLAKY_TESTS) \
		$(TEST_CI_ARGS) $(CI_NATIVE_SUITES)

# This target should not use a native compiler at all
test-ci-js: | clear-stalled
	$(PYTHON) tools/test.py $(PARALLEL_ARGS) -p tap --logfile test.tap \
		--mode=release --flaky-tests=$(FLAKY_TESTS) \
		$(TEST_CI_ARGS) $(CI_JS_SUITES)
	# Clean up any leftover processes, error if found.
	ps awwx | grep Release/node | grep -v grep | cat
	@PS_OUT=`ps awwx | grep Release/node | grep -v grep | awk '{print $$1}'`; \
	if [ "$${PS_OUT}" ]; then \
		echo $${PS_OUT} | $(XARGS) kill; exit 1; \
	fi

test-ci: LOGLEVEL := info
test-ci: | clear-stalled build-addons build-addons-napi
	out/Release/cctest --gtest_output=tap:cctest.tap
	$(PYTHON) tools/test.py $(PARALLEL_ARGS) -p tap --logfile test.tap \
		--mode=release --flaky-tests=$(FLAKY_TESTS) \
		$(TEST_CI_ARGS) $(CI_JS_SUITES) addons-napi $(CI_NATIVE_SUITES)
	# Clean up any leftover processes, error if found.
	ps awwx | grep Release/node | grep -v grep | cat
	@PS_OUT=`ps awwx | grep Release/node | grep -v grep | awk '{print $$1}'`; \
	if [ "$${PS_OUT}" ]; then \
		echo $${PS_OUT} | xargs kill; exit 1; \
	fi

test-release: test-build
	$(PYTHON) tools/test.py --mode=release

test-debug: test-build
	$(PYTHON) tools/test.py --mode=debug

test-message: test-build
	$(PYTHON) tools/test.py message

test-simple: | cctest  # Depends on 'all'.
	$(PYTHON) tools/test.py parallel sequential

test-pummel: all
	$(PYTHON) tools/test.py pummel

test-internet: all
	$(PYTHON) tools/test.py internet

test-debugger: all
	$(PYTHON) tools/test.py debugger

test-inspector: all
	$(PYTHON) tools/test.py inspector

test-tick-processor: all
	$(PYTHON) tools/test.py tick-processor

test-hash-seed: all
	$(NODE) test/pummel/test-hash-seed.js

test-known-issues: all
	$(PYTHON) tools/test.py known_issues

test-npm: $(NODE_EXE)
	NODE=$(NODE) tools/test-npm.sh

test-npm-publish: $(NODE_EXE)
	npm_package_config_publishtest=true $(NODE) deps/npm/test/run.js

test-addons-napi: test-build-addons-napi
	$(PYTHON) tools/test.py --mode=release addons-napi

test-addons: test-build test-addons-napi
	$(PYTHON) tools/test.py --mode=release addons

test-addons-clean:
	$(RM) -rf test/addons/??_*/
	$(RM) -rf test/addons/*/build
	$(RM) test/addons/.buildstamp test/addons/.docbuildstamp

test-timers:
	$(MAKE) --directory=tools faketime
	$(PYTHON) tools/test.py --mode=release timers

test-timers-clean:
	$(MAKE) --directory=tools clean


ifneq ("","$(wildcard deps/v8/tools/run-tests.py)")
test-v8: v8
#	note: performs full test unless QUICKCHECK is specified
	deps/v8/tools/run-tests.py --arch=$(V8_ARCH) \
        --mode=$(BUILDTYPE_LOWER) $(V8_TEST_OPTIONS) $(QUICKCHECK_ARG) \
        --no-presubmit \
        --shell-dir=$(PWD)/deps/v8/out/$(V8_ARCH).$(BUILDTYPE_LOWER) \
	 $(TAP_V8)
	git clean -fdxq -- deps/v8
	@echo Testing hash seed
	$(MAKE) test-hash-seed

test-v8-intl: v8
#	note: performs full test unless QUICKCHECK is specified
	deps/v8/tools/run-tests.py --arch=$(V8_ARCH) \
        --mode=$(BUILDTYPE_LOWER) --no-presubmit $(QUICKCHECK_ARG) \
        --shell-dir=deps/v8/out/$(V8_ARCH).$(BUILDTYPE_LOWER) intl \
        $(TAP_V8_INTL)

test-v8-benchmarks: v8
	deps/v8/tools/run-tests.py --arch=$(V8_ARCH) --mode=$(BUILDTYPE_LOWER) \
        --download-data $(QUICKCHECK_ARG) --no-presubmit \
        --shell-dir=deps/v8/out/$(V8_ARCH).$(BUILDTYPE_LOWER) benchmarks \
	 $(TAP_V8_BENCHMARKS)

test-v8-all: test-v8 test-v8-intl test-v8-benchmarks
# runs all v8 tests
else
test-v8 test-v8-intl test-v8-benchmarks test-v8-all:
	@echo "Testing v8 is not available through the source tarball."
	@echo "Use the git repo instead:" \
		"$ git clone https://github.com/nodejs/node.git"
endif

# Google Analytics ID used for tracking API docs page views, empty
# DOCS_ANALYTICS means no tracking scripts will be included in the
# generated .html files
DOCS_ANALYTICS ?=

apidoc_sources = $(wildcard doc/api/*.md)
apidocs_html = $(apidoc_dirs) $(apiassets) $(addprefix out/,$(apidoc_sources:.md=.html))
apidocs_json = $(apidoc_dirs) $(apiassets) $(addprefix out/,$(apidoc_sources:.md=.json))

apidoc_dirs = out/doc out/doc/api/ out/doc/api/assets

apiassets = $(subst api_assets,api/assets,$(addprefix out/,$(wildcard doc/api_assets/*)))

doc-only: $(apidocs_html) $(apidocs_json)
doc: $(NODE_EXE) doc-only

$(apidoc_dirs):
	mkdir -p $@

out/doc/api/assets/%: doc/api_assets/% out/doc/api/assets/
	cp $< $@

out/doc/%: doc/%
	cp -r $< $@

# check if ./node is actually set, else use user pre-installed binary
gen-json = tools/doc/generate.js --format=json $< > $@
out/doc/api/%.json: doc/api/%.md
	@[ -e tools/doc/node_modules/js-yaml/package.json ] || \
		[ -e tools/eslint/node_modules/js-yaml/package.json ] || \
		if [ -x $(NODE) ]; then \
			cd tools/doc && ../../$(NODE) ../../$(NPM) install; \
		else \
			cd tools/doc && node ../../$(NPM) install; \
		fi
	[ -x $(NODE) ] && $(NODE) $(gen-json) || node $(gen-json)

# check if ./node is actually set, else use user pre-installed binary
gen-html = tools/doc/generate.js --node-version=$(FULLVERSION) --format=html \
			--template=doc/template.html --analytics=$(DOCS_ANALYTICS) $< > $@
out/doc/api/%.html: doc/api/%.md
	@[ -e tools/doc/node_modules/js-yaml/package.json ] || \
		[ -e tools/eslint/node_modules/js-yaml/package.json ] || \
		if [ -x $(NODE) ]; then \
			cd tools/doc && ../../$(NODE) ../../$(NPM) install; \
		else \
			cd tools/doc && node ../../$(NPM) install; \
		fi
	[ -x $(NODE) ] && $(NODE) $(gen-html) || node $(gen-html)

docopen: $(apidocs_html)
	@$(PYTHON) -mwebbrowser file://$(PWD)/out/doc/api/all.html

docclean:
	$(RM) -r out/doc

build-ci:
	$(PYTHON) ./configure $(CONFIG_FLAGS)
	$(MAKE)

run-ci: build-ci
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
PLATFORM=$(shell uname | tr '[:upper:]' '[:lower:]')
NPMVERSION=v$(shell cat deps/npm/package.json | grep '"version"' | sed 's/^[^:]*: "\([^"]*\)",.*/\1/')

UNAME_M=$(shell uname -m)
ifeq ($(findstring x86_64,$(UNAME_M)),x86_64)
DESTCPU ?= x64
else
ifeq ($(findstring ppc64,$(UNAME_M)),ppc64)
DESTCPU ?= ppc64
else
ifeq ($(findstring ppc,$(UNAME_M)),ppc)
DESTCPU ?= ppc
else
ifeq ($(findstring s390x,$(UNAME_M)),s390x)
DESTCPU ?= s390x
else
ifeq ($(findstring s390,$(UNAME_M)),s390)
DESTCPU ?= s390
else
ifeq ($(findstring arm,$(UNAME_M)),arm)
DESTCPU ?= arm
else
ifeq ($(findstring aarch64,$(UNAME_M)),aarch64)
DESTCPU ?= aarch64
else
ifeq ($(findstring powerpc,$(shell uname -p)),powerpc)
DESTCPU ?= ppc64
else
DESTCPU ?= x86
endif
endif
endif
endif
endif
endif
endif
endif
ifeq ($(DESTCPU),x64)
ARCH=x64
else
ifeq ($(DESTCPU),arm)
ARCH=arm
else
ifeq ($(DESTCPU),aarch64)
ARCH=arm64
else
ifeq ($(DESTCPU),ppc64)
ARCH=ppc64
else
ifeq ($(DESTCPU),ppc)
ARCH=ppc
else
ifeq ($(DESTCPU),s390)
ARCH=s390
else
ifeq ($(DESTCPU),s390x)
ARCH=s390x
else
ARCH=x86
endif
endif
endif
endif
endif
endif
endif

# node and v8 use different arch names (e.g. node 'x86' vs v8 'ia32').
# pass the proper v8 arch name to $V8_ARCH based on user-specified $DESTCPU.
ifeq ($(DESTCPU),x86)
V8_ARCH=ia32
else
V8_ARCH ?= $(DESTCPU)

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
# Custom user-specified variation, use it directly
ifdef VARIATION
BINARYNAME=$(TARNAME)-$(PLATFORM)-$(ARCH)-$(VARIATION)
else
BINARYNAME=$(TARNAME)-$(PLATFORM)-$(ARCH)
endif
BINARYTAR=$(BINARYNAME).tar
# OSX doesn't have xz installed by default, http://macpkg.sourceforge.net/
XZ=$(shell which xz > /dev/null 2>&1; echo $$?)
XZ_COMPRESSION ?= 9e
PKG=$(TARNAME).pkg
MACOSOUTDIR=out/macos

release-only:
	@if [ "$(DISTTYPE)" != "nightly" ] && [ "$(DISTTYPE)" != "next-nightly" ] && \
		`grep -q REPLACEME doc/api/*.md`; then \
		echo 'Please update REPLACEME in Added: tags in doc/api/*.md (See doc/releases.md)' ; \
		exit 1 ; \
	fi
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
	$(RM) -r $(MACOSOUTDIR)
	mkdir -p $(MACOSOUTDIR)/installer/productbuild
	cat tools/macos-installer/productbuild/distribution.xml.tmpl  \
		| sed -E "s/\\{nodeversion\\}/$(FULLVERSION)/g" \
		| sed -E "s/\\{npmversion\\}/$(NPMVERSION)/g" \
	>$(MACOSOUTDIR)/installer/productbuild/distribution.xml ; \

	@for dirname in tools/macos-installer/productbuild/Resources/*/; do \
		lang=$$(basename $$dirname) ; \
		mkdir -p $(MACOSOUTDIR)/installer/productbuild/Resources/$$lang ; \
		printf "Found localization directory $$dirname\n" ; \
		cat $$dirname/welcome.html.tmpl  \
			| sed -E "s/\\{nodeversion\\}/$(FULLVERSION)/g" \
			| sed -E "s/\\{npmversion\\}/$(NPMVERSION)/g"  \
		>$(MACOSOUTDIR)/installer/productbuild/Resources/$$lang/welcome.html ; \
		cat $$dirname/conclusion.html.tmpl  \
			| sed -E "s/\\{nodeversion\\}/$(FULLVERSION)/g" \
			| sed -E "s/\\{npmversion\\}/$(NPMVERSION)/g"  \
		>$(MACOSOUTDIR)/installer/productbuild/Resources/$$lang/conclusion.html ; \
	done
	$(PYTHON) ./configure \
		--dest-cpu=x64 \
		--tag=$(TAG) \
		--release-urlbase=$(RELEASE_URLBASE) \
		$(CONFIG_FLAGS) $(BUILD_RELEASE_FLAGS)
	$(MAKE) install V=$(V) DESTDIR=$(MACOSOUTDIR)/dist/node
	SIGN="$(CODESIGN_CERT)" PKGDIR="$(MACOSOUTDIR)/dist/node/usr/local" bash \
		tools/osx-codesign.sh
	mkdir -p $(MACOSOUTDIR)/dist/npm/usr/local/lib/node_modules
	mkdir -p $(MACOSOUTDIR)/pkgs
	mv $(MACOSOUTDIR)/dist/node/usr/local/lib/node_modules/npm \
		$(MACOSOUTDIR)/dist/npm/usr/local/lib/node_modules
	unlink $(MACOSOUTDIR)/dist/node/usr/local/bin/npm
	$(NODE) tools/license2rtf.js < LICENSE > \
		$(MACOSOUTDIR)/installer/productbuild/Resources/license.rtf
	cp doc/osx_installer_logo.png $(MACOSOUTDIR)/installer/productbuild/Resources
	pkgbuild --version $(FULLVERSION) \
		--identifier org.nodejs.node.pkg \
		--root $(MACOSOUTDIR)/dist/node $(MACOSOUTDIR)/pkgs/node-$(FULLVERSION).pkg
	pkgbuild --version $(NPMVERSION) \
		--identifier org.nodejs.npm.pkg \
		--root $(MACOSOUTDIR)/dist/npm \
		--scripts ./tools/macos-installer/pkgbuild/npm/scripts \
			$(MACOSOUTDIR)/pkgs/npm-$(NPMVERSION).pkg
	productbuild --distribution $(MACOSOUTDIR)/installer/productbuild/distribution.xml \
		--resources $(MACOSOUTDIR)/installer/productbuild/Resources \
		--package-path $(MACOSOUTDIR)/pkgs ./$(PKG)
	SIGN="$(PRODUCTSIGN_CERT)" PKG="$(PKG)" bash tools/osx-productsign.sh

pkg: $(PKG)

pkg-upload: pkg
	ssh $(STAGINGSERVER) "mkdir -p nodejs/$(DISTTYPEDIR)/$(FULLVERSION)"
	chmod 664 $(TARNAME).pkg
	scp -p $(TARNAME).pkg $(STAGINGSERVER):nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/$(TARNAME).pkg
	ssh $(STAGINGSERVER) "touch nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/$(TARNAME).pkg.done"

$(TARBALL): release-only $(NODE_EXE) doc
	git checkout-index -a -f --prefix=$(TARNAME)/
	mkdir -p $(TARNAME)/doc/api
	cp doc/node.1 $(TARNAME)/doc/node.1
	cp -r out/doc/api/* $(TARNAME)/doc/api/
	$(RM) -r $(TARNAME)/deps/v8/{test,samples,tools/profviz,tools/run-tests.py}
	$(RM) -r $(TARNAME)/doc/images # too big
	$(RM) -r $(TARNAME)/deps/uv/{docs,samples,test}
	$(RM) -r $(TARNAME)/deps/openssl/openssl/{doc,demos,test}
	$(RM) -r $(TARNAME)/deps/zlib/contrib # too big, unused
	$(RM) -r $(TARNAME)/.{editorconfig,git*,mailmap}
	$(RM) -r $(TARNAME)/tools/{eslint,eslint-rules,osx-pkg.pmdoc,pkgsrc}
	$(RM) -r $(TARNAME)/tools/{osx-*,license-builder.sh,cpplint.py}
	$(RM) -r $(TARNAME)/test*.tap
	find $(TARNAME)/ -name ".eslint*" -maxdepth 2 | xargs $(RM)
	find $(TARNAME)/ -type l | xargs $(RM) # annoying on windows
	tar -cf $(TARNAME).tar $(TARNAME)
	$(RM) -r $(TARNAME)
	gzip -c -f -9 $(TARNAME).tar > $(TARNAME).tar.gz
ifeq ($(XZ), 0)
	xz -c -f -$(XZ_COMPRESSION) $(TARNAME).tar > $(TARNAME).tar.xz
endif
	$(RM) $(TARNAME).tar

tar: $(TARBALL)

tar-upload: tar
	ssh $(STAGINGSERVER) "mkdir -p nodejs/$(DISTTYPEDIR)/$(FULLVERSION)"
	chmod 664 $(TARNAME).tar.gz
	scp -p $(TARNAME).tar.gz $(STAGINGSERVER):nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/$(TARNAME).tar.gz
	ssh $(STAGINGSERVER) "touch nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/$(TARNAME).tar.gz.done"
ifeq ($(XZ), 0)
	chmod 664 $(TARNAME).tar.xz
	scp -p $(TARNAME).tar.xz $(STAGINGSERVER):nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/$(TARNAME).tar.xz
	ssh $(STAGINGSERVER) "touch nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/$(TARNAME).tar.xz.done"
endif

doc-upload: doc
	ssh $(STAGINGSERVER) "mkdir -p nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/docs/"
	chmod -R ug=rw-x+X,o=r+X out/doc/
	scp -pr out/doc/* $(STAGINGSERVER):nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/docs/
	ssh $(STAGINGSERVER) "touch nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/docs.done"

$(TARBALL)-headers: release-only
	$(PYTHON) ./configure \
		--prefix=/ \
		--dest-cpu=$(DESTCPU) \
		--tag=$(TAG) \
		--release-urlbase=$(RELEASE_URLBASE) \
		$(CONFIG_FLAGS) $(BUILD_RELEASE_FLAGS)
	HEADERS_ONLY=1 $(PYTHON) tools/install.py install '$(TARNAME)' '/'
	find $(TARNAME)/ -type l | xargs $(RM)
	tar -cf $(TARNAME)-headers.tar $(TARNAME)
	$(RM) -r $(TARNAME)
	gzip -c -f -9 $(TARNAME)-headers.tar > $(TARNAME)-headers.tar.gz
ifeq ($(XZ), 0)
	xz -c -f -$(XZ_COMPRESSION) $(TARNAME)-headers.tar > $(TARNAME)-headers.tar.xz
endif
	$(RM) $(TARNAME)-headers.tar

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
	$(RM) -r $(BINARYNAME)
	$(RM) -r out/deps out/Release
	$(PYTHON) ./configure \
		--prefix=/ \
		--dest-cpu=$(DESTCPU) \
		--tag=$(TAG) \
		--release-urlbase=$(RELEASE_URLBASE) \
		$(CONFIG_FLAGS) $(BUILD_RELEASE_FLAGS)
	$(MAKE) install DESTDIR=$(BINARYNAME) V=$(V) PORTABLE=1
	cp README.md $(BINARYNAME)
	cp LICENSE $(BINARYNAME)
	cp CHANGELOG.md $(BINARYNAME)
ifeq ($(OSTYPE),darwin)
	SIGN="$(CODESIGN_CERT)" PKGDIR="$(BINARYNAME)" bash tools/osx-codesign.sh
endif
	tar -cf $(BINARYNAME).tar $(BINARYNAME)
	$(RM) -r $(BINARYNAME)
	gzip -c -f -9 $(BINARYNAME).tar > $(BINARYNAME).tar.gz
ifeq ($(XZ), 0)
	xz -c -f -$(XZ_COMPRESSION) $(BINARYNAME).tar > $(BINARYNAME).tar.xz
endif
	$(RM) $(BINARYNAME).tar

binary: $(BINARYTAR)

binary-upload: binary
	ssh $(STAGINGSERVER) "mkdir -p nodejs/$(DISTTYPEDIR)/$(FULLVERSION)"
	chmod 664 $(TARNAME)-$(OSTYPE)-$(ARCH).tar.gz
	scp -p $(TARNAME)-$(OSTYPE)-$(ARCH).tar.gz $(STAGINGSERVER):nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/$(TARNAME)-$(OSTYPE)-$(ARCH).tar.gz
	ssh $(STAGINGSERVER) "touch nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/$(TARNAME)-$(OSTYPE)-$(ARCH).tar.gz.done"
ifeq ($(XZ), 0)
	chmod 664 $(TARNAME)-$(OSTYPE)-$(ARCH).tar.xz
	scp -p $(TARNAME)-$(OSTYPE)-$(ARCH).tar.xz $(STAGINGSERVER):nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/$(TARNAME)-$(OSTYPE)-$(ARCH).tar.xz
	ssh $(STAGINGSERVER) "touch nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/$(TARNAME)-$(OSTYPE)-$(ARCH).tar.xz.done"
endif

haswrk=$(shell which wrk > /dev/null 2>&1; echo $$?)
wrk:
ifneq ($(haswrk), 0)
	@echo "please install wrk before proceeding. More information can be found in benchmark/README.md." >&2
	@exit 1
endif

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

bench-url: all
	@$(NODE) benchmark/common.js url

bench-events: all
	@$(NODE) benchmark/common.js events

bench-util: all
	@$(NODE) benchmark/common.js util

bench-dgram: all
	@$(NODE) benchmark/common.js dgram

bench-all: bench bench-misc bench-array bench-buffer bench-url bench-events bench-dgram bench-util

bench: bench-net bench-http bench-fs bench-tls

bench-ci: bench

bench-http-simple:
	benchmark/http_simple_bench.sh

bench-idle:
	$(NODE) benchmark/idle_server.js &
	sleep 1
	$(NODE) benchmark/idle_clients.js &

lint-js:
	@echo "Running JS linter..."
	$(NODE) tools/eslint/bin/eslint.js --cache --rulesdir=tools/eslint-rules --ext=.js,.md \
	  benchmark doc lib test tools

jslint: lint-js

lint-js-ci:
	@echo "Running JS linter..."
	$(NODE) tools/lint-js.js $(PARALLEL_ARGS) -f tap -o test-eslint.tap \
		benchmark doc lib test tools

jslint-ci: lint-js-ci

LINT_CPP_EXCLUDE ?=
LINT_CPP_EXCLUDE += src/node_root_certs.h
LINT_CPP_EXCLUDE += src/queue.h
LINT_CPP_EXCLUDE += src/tree.h
LINT_CPP_EXCLUDE += $(wildcard test/addons/??_*/*.cc test/addons/??_*/*.h)

LINT_CPP_FILES = $(filter-out $(LINT_CPP_EXCLUDE), $(wildcard \
	src/*.c \
	src/*.cc \
	src/*.h \
	test/addons/*/*.cc \
	test/addons/*/*.h \
	test/cctest/*.cc \
	test/cctest/*.h \
	test/addons-napi/*/*.cc \
	test/addons-napi/*/*.h \
	tools/icu/*.cc \
	tools/icu/*.h \
	))

lint-cpp:
	@echo "Running C++ linter..."
	@$(PYTHON) tools/cpplint.py $(LINT_CPP_FILES)
	@$(PYTHON) tools/check-imports.py

cpplint: lint-cpp

ifneq ("","$(wildcard tools/eslint/)")
lint:
	@EXIT_STATUS=0 ; \
	$(MAKE) lint-js || EXIT_STATUS=$$? ; \
	$(MAKE) lint-cpp || EXIT_STATUS=$$? ; \
	exit $$EXIT_STATUS
CONFLICT_RE=^>>>>>>> [0-9A-Fa-f]+|^<<<<<<< [A-Za-z]+
lint-ci: lint-js-ci lint-cpp
	@if ! ( grep -IEqrs "$(CONFLICT_RE)" benchmark deps doc lib src test tools ) \
		&& ! ( find . -maxdepth 1 -type f | xargs grep -IEqs "$(CONFLICT_RE)" ); then \
		exit 0 ; \
	else \
		echo "" >&2 ; \
		echo "Conflict marker detected in one or more files. Please fix them first." >&2 ; \
		exit 1 ; \
	fi
else
lint:
	@echo "Linting is not available through the source tarball."
	@echo "Use the git repo instead:" \
		"$ git clone https://github.com/nodejs/node.git"

lint-ci: lint
endif

.PHONY: lint lint-cpp lint-js bench clean docopen docclean doc dist distclean \
	check uninstall install install-includes install-bin all staticlib \
	dynamiclib test test-all test-addons test-addons-clean build-addons \
        website-upload pkg blog blogclean tar binary release-only \
        bench-http-simple bench-idle bench-all bench bench-misc bench-array \
        bench-buffer bench-net bench-http bench-fs bench-tls cctest run-ci \
        test-v8 test-v8-intl test-v8-benchmarks test-v8-all v8 lint-ci \
        bench-ci lint-js-ci doc-only $(TARBALL)-headers test-ci test-ci-native \
        test-ci-js build-ci test-hash-seed clear-stalled  test-addons-napi \
        build-addons-napi

