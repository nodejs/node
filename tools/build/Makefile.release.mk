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
DESTCPU ?= arm64
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
ifeq ($(DESTCPU),arm64)
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
HAS_XZ ?= $(shell which xz > /dev/null 2>&1; [ $$? -eq 0 ] && echo 1 || echo 0)
# Supply SKIP_XZ=1 to explicitly skip .tar.xz creation
SKIP_XZ ?= 0
XZ = $(shell [ $(HAS_XZ) -eq 1 -a $(SKIP_XZ) -eq 0 ] && echo 1 || echo 0)
XZ_COMPRESSION ?= 9e
PKG=$(TARNAME).pkg
MACOSOUTDIR=out/macos

ifeq ($(SKIP_XZ), 1)
check-xz:
	@echo "SKIP_XZ=1 supplied, skipping .tar.xz creation"
else
ifeq ($(HAS_XZ), 1)
check-xz:
else
check-xz:
	@echo "No xz command, cannot continue"
	@exit 1
endif
endif
DEST_DIR := nodejs/$(DISTTYPEDIR)/$(FULLVERSION)

define do_upload =
	ssh $(STAGINGSERVER) "mkdir -p $(DEST_DIR)"
	chmod 664 $(1)
	scp -p $^ $(STAGINGSERVER):$(DEST_DIR)/$(1)
	ssh $(STAGINGSERVER) "touch $(DEST_DIR)/$(1).done"
	ssh $(STAGINGSERVER) "touch $(DEST_DIR)/$(1)*"
endef
do_upload_recepie = $(call do_upload, $^)

.PHONY: release-only
release-only: check-xz
	@if [ "$(DISTTYPE)" = "release" ] && `grep -q REPLACEME doc/api/*.md`; then \
		echo 'Please update REPLACEME in Added: tags in doc/api/*.md (See doc/releases.md)' ; \
		exit 1 ; \
	fi
	@if [ "$(DISTTYPE)" = "release" ] && \
		`grep -q DEP...X doc/api/deprecations.md`; then \
		echo 'Please update DEP...X in doc/api/deprecations.md (See doc/releases.md)' ; \
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
	unlink $(MACOSOUTDIR)/dist/node/usr/local/bin/npx
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

# Builds the macOS installer for releases.
# Note: this is strictly for release builds on release machines only.
.PHONY: $(PKG)
pkg-upload: $(PKG)
	$(do_upload_recepie)

$(TARBALL): release-only $(NODE_EXE) doc
	git checkout-index -a -f --prefix=$(TARNAME)/
	mkdir -p $(TARNAME)/doc/api
	cp doc/node.1 $(TARNAME)/doc/node.1
	cp -r out/doc/api/* $(TARNAME)/doc/api/
	$(RM) -r $(TARNAME)/.editorconfig
	$(RM) -r $(TARNAME)/.git*
	$(RM) -r $(TARNAME)/.mailmap
	$(RM) -r $(TARNAME)/deps/openssl/openssl/demos
	$(RM) -r $(TARNAME)/deps/openssl/openssl/doc
	$(RM) -r $(TARNAME)/deps/openssl/openssl/test
	$(RM) -r $(TARNAME)/deps/uv/docs
	$(RM) -r $(TARNAME)/deps/uv/samples
	$(RM) -r $(TARNAME)/deps/uv/test
	$(RM) -r $(TARNAME)/deps/v8/samples
	$(RM) -r $(TARNAME)/deps/v8/test
	$(RM) -r $(TARNAME)/deps/v8/tools/profviz
	$(RM) -r $(TARNAME)/deps/v8/tools/run-tests.py
	$(RM) -r $(TARNAME)/deps/zlib/contrib # too big, unused
	$(RM) -r $(TARNAME)/doc/images # too big
	$(RM) -r $(TARNAME)/test*.tap
	$(RM) -r $(TARNAME)/tools/cpplint.py
	$(RM) -r $(TARNAME)/tools/eslint-rules
	$(RM) -r $(TARNAME)/tools/license-builder.sh
	$(RM) -r $(TARNAME)/tools/node_modules
	$(RM) -r $(TARNAME)/tools/osx-*
	$(RM) -r $(TARNAME)/tools/osx-pkg.pmdoc
	$(RM) -r $(TARNAME)/tools/pkgsrc
	find $(TARNAME)/ -name ".eslint*" -maxdepth 2 | xargs $(RM)
	find $(TARNAME)/ -type l | xargs $(RM) # annoying on windows
	tar -cf $(TARNAME).tar $(TARNAME)
	$(RM) -r $(TARNAME)
	gzip -c -f -9 $(TARNAME).tar > $(TARNAME).tar.gz
ifeq ($(XZ), 1)
	xz -c -f -$(XZ_COMPRESSION) $(TARNAME).tar > $(TARNAME).tar.xz
endif
	$(RM) $(TARNAME).tar


# Note: this is strictly for release builds on release machines only.
.PHONY: tar-upload
tar-upload: $(TARBALL)
	$(do_upload_recepie)
ifeq ($(XZ), 1)
	$(call do_upload $(TARNAME).tar.xz)
endif

# Note: this is strictly for release builds on release machines only.
doc-upload: doc
	ssh $(STAGINGSERVER) "mkdir -p $(DEST_DIR)/docs/"
	chmod -R ug=rw-x+X,o=r+X out/doc/
	scp -pr out/doc/* $(STAGINGSERVER):$(DEST_DIR)/docs/
	ssh $(STAGINGSERVER) "touch $(DEST_DIR)/docs.done"
	ssh $(STAGINGSERVER) "find '$(DEST_DIR)/docs' -name '*' | xargs touch"

.PHONY: $(TARBALL)-headers
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
ifeq ($(XZ), 1)
	xz -c -f -$(XZ_COMPRESSION) $(TARNAME)-headers.tar > $(TARNAME)-headers.tar.xz
endif
	$(RM) $(TARNAME)-headers.tar

.PHONEY: tar-headers-upload
tar-headers-upload: $(TARBALL)-headers
	$(do_upload_recepie)
ifeq ($(XZ), 1)
	$(call do_upload $(TARNAME)-headers.tar.xz)
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
ifeq ($(XZ), 1)
	xz -c -f -$(XZ_COMPRESSION) $(BINARYNAME).tar > $(BINARYNAME).tar.xz
endif
	$(RM) $(BINARYNAME).tar

# This requires NODE_VERSION_IS_RELEASE defined as 1 in src/node_version.h.
# Note: this is strictly for release builds on release machines only.
.PHONY: binary-upload
binary-upload: $(BINARYTAR)
	$(do_upload_recepie)
ifeq ($(XZ), 1)
	$(call do_upload $(TARNAME)-$(OSTYPE)-$(ARCH).tar.xz)
endif

