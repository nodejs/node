#config

# define DEBUG=1 to build node_g

WANT_OPENSSL=1
PREFIX=/usr
DESTDIR=
SHELL=/bin/sh
INSTALL = install
SCONS = python tools/scons/scons.py
LINKFLAGS += $(LDFLAGS)

-include config.mak.autogen
-include config.mak

# -----------------------------------------------------------------------------

dylib_suffix = so

ifeq ($(platform),darwin)
	LINKFLAGS += -framework Carbon
	dylib_suffix = dylib
endif

ifeq ($(platform),linux)
	LINKFLAGS += -pthread -lrt -rdynamic
endif

ifeq ($(platform),solaris)
	WANT_SOCKET = 1
endif

ifdef WANT_OPENSSL
	HAVE_OPENSSL = 1
	HAVE_CRYPTO = 1
	ifdef OPENSSL_DIR
		OPENSSL_LINKFLAGS += -L$(OPENSSL_DIR)/lib
		OPENSSL_CPPFLAGS += -I$(OPENSSL_DIR)/include
	endif
	OPENSSL_LINKFLAGS += -lssl -lcrypto
endif

ifdef WANT_SOCKET
	LINKFLAGS += -lsocket -lnsl
endif

ifneq (,$(findstring build/libnode.so,$(MAKEFLAGS)))
	CFLAGS += -shared -fPIC
#else
#	CFLAGS += $(MAKEFLAGS)
endif

cflags += -pedantic



debug_CPPFLAGS = -DDEBUG $(CPPFALGS)
debug_CFLAGS = -Wall -O0 -ggdb
debug_CXXFLAGS = $(debug_CFLAGS)
debug_LINKFLAGS = $(LINKFLAGS)

release_CPPFLAGS = -DNODEBUG $(CPPFALGS)
release_CFLAGS = -Wall -O2 $(CFLAGS)
release_CXXFLAGS = $(release_CFLAGS)
release_LINKFLAGS = $(LINKFLAGS)

builddir = build


libev_sources = deps/libev/ev.c
# Note: -I$(builddir)/deps/libev contains config.h which is generated from
# deps/libev/config.h.in during the configure script
libev_CPPFLAGS = -Ideps/libev -I$(builddir)/deps/libev
libev_release_objects = $(builddir)/release/deps/libev/ev.o
libev_debug_objects = $(builddir)/debug/deps/libev/ev.o

libeio_sources = deps/libeio/eio.c
libeio_release_objects = $(builddir)/release/deps/libeio/eio.o
libeio_debug_objects = $(builddir)/debug/deps/libeio/eio.o
# Note: -I$(builddir)/deps/libeio contains config.h which is generated from
# deps/libeio/config.h.in during the configure script
libeio_CPPFLAGS = -D_GNU_SOURCE -Ideps/libeio -I$(builddir)/deps/libeio

http_parser_sources = deps/http_parser/http_parser.c
http_parser_release_objects = $(builddir)/release/deps/http_parser/http_parser.o
http_parser_debug_objects = $(builddir)/debug/deps/http_parser/http_parser.o
http_parser_CPPFLAGS = -Ideps/http_parser

cares_sources = $(wildcard deps/c-ares/*.c)
cares_release_objects = $(addprefix $(builddir)/release/,$(cares_sources:.c=.o))
cares_debug_objects = $(addprefix $(builddir)/debug/,$(cares_sources:.c=.o))
cares_CPPFLAGS = -DHAVE_CONFIG_H=1 -Ideps/c-ares -Ideps/c-ares/$(platform)-$(arch)

node_sources = src/node.cc \
	src/platform_$(platform).cc \
	src/node_buffer.cc \
	src/node_cares.cc \
	src/node_child_process.cc \
	src/node_constants.cc \
	src/node_crypto.cc \
	src/node_events.cc \
	src/node_extensions.cc \
	src/node_file.cc \
	src/node_http_parser.cc \
	src/node_idle_watcher.cc \
	src/node_io_watcher.cc \
	src/node_net.cc \
	src/node_script.cc \
	src/node_signal_watcher.cc \
	src/node_stat_watcher.cc \
	src/node_stdio.cc \
	src/node_timer.cc \
	src/node_javascript.cc \

node_install_includes = src/node.h \
	src/node_object_wrap.h \
	src/node_buffer.h \
	src/node_events.h \
	src/node_version.h \
	deps/libeio/eio.h \
	deps/libev/ev.h \
	deps/v8/include/*.h \

ifdef DEBUG
node_install_includes += $(builddir)/debug/src/node_config.h
else
node_install_includes += $(builddir)/release/src/node_config.h
endif


node_debug_objects = $(addprefix $(builddir)/debug/,$(node_sources:.cc=.o))
node_release_objects = $(addprefix $(builddir)/release/,$(node_sources:.cc=.o))

node_exe_sources = src/node_main.cc

node_exe_debug_objects = \
    $(addprefix $(builddir)/debug/,$(node_exe_sources:.cc=.o))
node_exe_release_objects = \
    $(addprefix $(builddir)/release/,$(node_exe_sources:.cc=.o))

# TODO HAVE_FDATASYNC should be set in configure.

node_CPPFLAGS = -Isrc/ -Ideps/libeio/ -Ideps/libev/ -Ideps/http_parser/ \
	-Ideps/libev/include/ -Ideps/v8/include -DPLATFORM=\"$(platform)\" \
	-DX_STACKSIZE=65536 -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 \
	-DHAVE_OPENSSL=${HAVE_OPENSSL} \
	-DHAVE_FDATASYNC=0 -I$(builddir)/release/src $(cares_CPPFLAGS)
node_debug_CPPFLAGS = $(subst release,debug,$(NODE_CPPFLAGS))

libv8 = $(builddir)/libv8.a
libv8_g = $(builddir)/libv8_g.a



# Rules

all: node

ifdef DEBUG
all: node_g
endif


node: $(builddir)/node
	ln -fs $< $@

node_g: $(builddir)/node_g
	ln -fs $< $@



# libev

$(builddir)/release/deps/libev/%.o: deps/libev/%.c 
	$(CC) -c $(release_CFLAGS) $(release_CPPFLAGS) $(libev_CFLAGS) \
		$(libev_CPPFLAGS) $< -o $@

$(builddir)/debug/deps/libev/%.o: deps/libev/%.c 
	$(CC) -c $(debug_CFLAGS) $(debug_CPPFLAGS) $(libev_CFLAGS) \
		$(libev_CPPFLAGS) $< -o $@


# libeio

$(builddir)/release/deps/libeio/%.o: deps/libeio/%.c 
	$(CC) -c $(release_CFLAGS) $(release_CPPFLAGS) $(libeio_CFLAGS) \
		$(libeio_CPPFLAGS) $< -o $@

$(builddir)/debug/deps/libeio/%.o: deps/libeio/%.c 
	$(CC) -c $(debug_CFLAGS) $(debug_CPPFLAGS) $(libeio_CFLAGS) \
		$(libeio_CPPFLAGS) $< -o $@


# http-parser

$(builddir)/release/deps/http_parser/%.o: deps/http_parser/%.c 
	$(CC) -c $(release_CFLAGS) $(release_CPPFLAGS) $(http_parser_CFLAGS) \
		$(http_parser_CPPFLAGS) $< -o $@

$(builddir)/debug/deps/http_parser/%.o: deps/http_parser/%.c 
	$(CC) -c $(debug_CFLAGS) $(debug_CPPFLAGS) $(http_parser_CFLAGS) \
		$(http_parser_CPPFLAGS) $< -o $@


# c-ares

$(builddir)/release/deps/c-ares/%.o: deps/c-ares/%.c 
	$(CC) -c $(release_CFLAGS) $(release_CPPFLAGS) $(cares_CFLAGS) \
		$(cares_CPPFLAGS) $< -o $@

$(builddir)/debug/deps/c-ares/%.o: deps/c-ares/%.c 
	$(CC) -c $(debug_CFLAGS) $(debug_CPPFLAGS) $(cares_CFLAGS) \
		$(cares_CPPFLAGS) $< -o $@


# node

$(builddir)/release/src/%.o: src/%.cc
	$(CXX) -c $(release_CXXFLAGS) $(release_CPPFLAGS) $(node_CXXFLAGS) \
		$(node_CPPFLAGS) $(OPENSSL_CPPFLAGS) $< -o $@

$(builddir)/debug/src/%.o: src/%.cc
	$(CXX) -c $(debug_CXXFLAGS) $(debug_CPPFLAGS) $(node_CXXFLAGS) \
		$(node_CPPFLAGS) $(OPENSSL_CPPFLAGS) $< -o $@


# node.o

$(builddir)/release/src/node.o: src/node.cc $(builddir)/release/src/node_natives.h
	$(CXX) -c $(release_CXXFLAGS) $(release_CPPFLAGS) $(node_CFLAGS) \
		$(node_CPPFLAGS) $(OPENSSL_CPPFLAGS) $< -o $@

$(builddir)/debug/src/node.o: src/node.cc $(builddir)/debug/src/node_natives.h
	$(CXX) -c $(debug_CXXFLAGS) $(debug_CPPFLAGS) $(node_CFLAGS) \
		$(node_CPPFLAGS) $(OPENSSL_CPPFLAGS) $< -o $@


# node_javascript.o

$(builddir)/release/src/node_javascript.o: src/node_javascript.cc $(builddir)/release/src/node_natives.h
	$(CXX) -c $(release_CXXFLAGS) $(release_CPPFLAGS) $(node_CFLAGS) \
		$(node_CPPFLAGS) $(OPENSSL_CPPFLAGS) $< -o $@

$(builddir)/debug/src/node_javascript.o: src/node_javascript.cc $(builddir)/debug/src/node_natives.h
	$(CXX) -c $(debug_CXXFLAGS) $(debug_CPPFLAGS) $(node_CFLAGS) \
		$(node_CPPFLAGS) $(OPENSSL_CPPFLAGS) $< -o $@

# -----------------------------------------------------------------------------
# end products

endproduct_release_objects = $(node_release_objects) \
		$(libev_release_objects) $(libeio_release_objects) \
		$(http_parser_release_objects) $(cares_release_objects) $(libv8)

endproduct_debug_objects = $(node_debug_objects) \
		$(libev_debug_objects) $(libeio_debug_objects) \
		$(http_parser_debug_objects) $(cares_debug_objects) $(libv8_g)

endproduct_release_linkflags = $(release_LINKFLAGS) $(node_LINKFLAGS) \
		$(OPENSSL_LINKFLAGS)

endproduct_debug_linkflags = $(debug_LINKFLAGS) $(node_LINKFLAGS) \
		$(OPENSSL_LINKFLAGS)



# node executable
$(builddir)/node: $(node_exe_release_objects) $(endproduct_release_objects)
	$(CXX) -o $@ $^ $(endproduct_release_linkflags)

$(builddir)/node_g: $(node_exe_debug_objects) $(endproduct_debug_objects)
	$(CXX) -o $@ $^ $(endproduct_debug_linkflags)

# node static library
$(builddir)/libnode.a: $(endproduct_release_objects)
	ar -rcs $@ $^

$(builddir)/libnode_g.a: $(endproduct_debug_objects)
	ar -rcs $@ $^

# node dynamic library
# WIP -- currently disabled
#$(builddir)/libnode.$(dylib_suffix): CFLAGS += -shared -fPIC
#$(builddir)/libnode.$(dylib_suffix): $(endproduct_release_objects)
#	$(CXX) -o $@ $^ $(endproduct_release_linkflags)
#
#$(builddir)/libnode_g.$(dylib_suffix): CFLAGS += -shared -fPIC
#$(builddir)/libnode_g.$(dylib_suffix): $(endproduct_debug_objects)
#	$(CXX) -o $@ $^ $(endproduct_debug_linkflags)

# built-in javascript (the "node standard library")
$(builddir)/release/src/node_natives.h: src/node.js lib/*.js
	python tools/js2c.py $^ > $@

$(builddir)/debug/src/node_natives.h: src/node.js lib/*.js
	python tools/js2c.py $^ > $@
	# TODO a debug flag for the macros ?



$(builddir)/release/src/node_config.h: src/node_config.h.in
	sed -e "s#@PREFIX@#$(PREFIX)#" \
		-e "s#@CCFLAGS@#$(release_CFLAGS)#" \
		-e "s#@CPPFLAGS@#$(release_CPPFLAGS)#" $< > $@ || rm $@

$(builddir)/debug/src/node_config.h: src/node_config.h.in
	sed -e "s#@PREFIX@#$(PREFIX)#" \
		-e "s#@CCFLAGS@#$(debug_CFLAGS)#" \
		-e "s#@CPPFLAGS@#$(debug_CPPFLAGS)#" $< > $@ || rm $@


# FIXME convert to a generalized *.in preprocessor
$(builddir)/release/lib/pkgconfig/nodejs.pc: tools/nodejs.pc.in
	sed \
		-e "s#@PREFIX@#$(PREFIX)#" \
		-e "s#@VERSION@#$(VERSION)#" \
		-e "s#@CCFLAGS@#$(CFLAGS)#" \
		-e "s#@CPPFLAGS@#$(CPPFLAGS)#" $< > $@ || rm $@

# v8 does its own debug and release version, so we don't put it in the
# profile_builddir but rather just the builddir.
libv8_flags = --directory="$(builddir)" --srcdir="`pwd`/deps/v8" \
		visibility=default arch=$(arch) library=static
# TODO: if env[SNAPSHOT_V8]; then
libv8_flags += snapshot=on
#fi
$(libv8):
	$(SCONS) $(libv8_flags) mode=release

$(libv8_g):
	$(SCONS) $(libv8_flags) mode=debug


# header deps
$(builddir)/release/src/node.o: $(builddir)/release/src/node_config.h
$(builddir)/debug/src/node.o: $(builddir)/debug/src/node_config.h


# TODO install libs
#install: all doc
install: all
	$(INSTALL) -d -m 755 '$(DESTDIR)$(PREFIX)/bin'
	$(INSTALL) $(builddir)/node '$(DESTDIR)$(PREFIX)/bin'
	$(INSTALL) -d -m 755 '$(DESTDIR)$(PREFIX)/include/node'
	$(INSTALL) $(node_install_includes) '$(DESTDIR)$(PREFIX)/include/node'
	$(INSTALL) -d -m 755 '$(DESTDIR)$(PREFIX)/share/man/man1/'
	$(INSTALL) -d -m 755 '$(DESTDIR)$(PREFIX)/lib/node/wafadmin/Tools'
	$(INSTALL) tools/wafadmin/*.py '$(DESTDIR)$(PREFIX)/lib/node/wafadmin'
	$(INSTALL) tools/wafadmin/Tools/*.py '$(DESTDIR)$(PREFIX)/lib/node/wafadmin/Tools'
	$(INSTALL) tools/node-waf '$(DESTDIR)$(PREFIX)/bin'
#	$(INSTALL) doc/node.1 '$(DESTDIR)$(PREFIX)/share/man/man1/'

libnode-static: $(builddir)/libnode.a
	ln -fs $< $@

libnode-static-debug: $(builddir)/libnode_g.a
	ln -fs $< $@

libnode-dynamic: $(builddir)/libnode.$(dylib_suffix)
	ln -fs $< $@

libnode-dynamic-debug: $(builddir)/libnode_g.$(dylib_suffix)
	ln -fs $< $@



test: $(builddir)/node
	python tools/test.py --mode=release simple message

test-all: $(builddir)/node $(builddir)/node_g
	python tools/test.py --mode=debug,release

test-release: $(builddir)/node
	python tools/test.py --mode=release

test-debug: $(builddir)/node_g
	python tools/test.py --mode=debug

test-message: $(builddir)/node
	python tools/test.py message

test-simple: $(builddir)/node
	python tools/test.py simple
     
test-pummel: $(builddir)/node
	python tools/test.py pummel
	
test-internet: $(builddir)/node
	python tools/test.py internet


doc: $(builddir)/doc/api/all.html $(builddir)/doc/changelog.html

docopen: $(builddir)/doc/api/all.html
	-google-chrome $(builddir)/doc/api/all.html

$(builddir)/doc/api/all.html: $(builddir)/node  doc/api/*.markdown 
	$(builddir)/node tools/doctool/doctool.js

$(builddir)/doc/changelog.html: ChangeLog doc/changelog_header.html \
		doc/changelog_footer.html
	cat doc/changelog_header.html ChangeLog doc/changelog_footer.html > $(builddir)/doc/changelog.html

# $(buildir)/doc/node.1: $(builddir)/node doc/api.markdown all
# 	$(builddir)/node tools/ronnjs/bin/ronn.js --roff doc/api.markdown > $(builddir)/doc/node.1

website-upload: doc
	scp doc/* ryan@nodejs.org:~/web/nodejs.org/

docclean:
	-rm -rf $(builddir)/doc

clean:
	-rm -f node node_g $(builddir)/node $(builddir)/node_g $(libv8) $(libv8_g)
	-rm -f $(node_release_objects) $(node_debug_objects)
	-rm -f $(cares_release_objects) $(cares_debug_objects)
	-rm -f $(http_parser_release_objects) $(http_parser_debug_objects)
	-rm -f $(libev_release_objects) $(libev_debug_objects)
	-rm -f $(libeio_release_objects) $(libeio_debug_objects)
	-find . -name "*.pyc" | xargs rm -f

distclean: docclean
	-find tools -name "*.pyc" | xargs rm -f
	-rm -rf build/ node node_g
	-rm -rf configure.real config.mak.autogen config.log autom4te.cache config.status


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


.PHONY: all \
  test test-all \
  bench \
  install uninstall \
  dist distclean \
  website-upload \
  clean docclean docopen

