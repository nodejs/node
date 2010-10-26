#config

# define DEBUG=1 to build node_g

WANT_OPENSSL=1
PREFIX=/usr
SHELL=/bin/sh
INSTALL = install


-include config.mak.autogen
-include config.mak


platform := $(shell python -c 'import sys; print sys.platform')


ifeq ($(platform),linux2)
	platform := linux
endif

# fix me
arch = x86_64


ifeq ($(platform),darwin)
	LINKFLAGS += -framework Carbon
endif

ifeq ($(platform),linux)
	LINKFLAGS += -pthread -lrt
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

cflags += -pedantic




debug_CPPDEFINES = -DDEBUG $(CFLAGS)
debug_CFLAGS = -Wall -O0 -ggdb $(CFLAGS)
debug_CXXFLAGS = $(debug_CFLAGS)
debug_LINKFLAGS = $(LINKFLAGS)

release_CPPDEFINES = -DNODEBUG
release_CFLAGS = -Wall -O2
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
	src/node_main.cc \
	src/node_net.cc \
	src/node_script.cc \
	src/node_signal_watcher.cc \
	src/node_stat_watcher.cc \
	src/node_stdio.cc \
	src/node_timer.cc \
	src/node_javascript.cc \

node_debug_objects = $(addprefix $(builddir)/debug/,$(node_sources:.cc=.o))
node_release_objects = $(addprefix $(builddir)/release/,$(node_sources:.cc=.o))

# TODO HAVE_FDATASYNC should be set in configure.

node_CPPFLAGS = -Isrc/ -Ideps/libeio/ -Ideps/libev/ -Ideps/http_parser/ \
	-Ideps/libev/include/ -Ideps/v8/include -DPLATFORM=\"$(platform)\" \
	-DX_STACKSIZE=65536 -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 \
	-DHAVE_OPENSSL=${HAVE_OPENSSL} \
	-DHAVE_FDATASYNC=0 -I$(builddir)/release/src $(cares_CPPFLAGS)
node_debug_CPPFLAGS = $(subst release,debug,$(NODE_CPPFLAGS))

libv8 = $(builddir)/libv8.a
libv8_g = $(builddir)/libv8_g.a

dirs = $(builddir)/release/src \
	$(builddir)/release/deps/libev \
	$(builddir)/release/deps/libeio \
	$(builddir)/release/deps/c-ares \
	$(builddir)/release/deps/http_parser \
	$(builddir)/release/deps/v8 \
	$(builddir)/release/lib/pkgconfig
debug_dirs = $(subst release,debug,$(dirs))




# Rules

all: $(dirs) node

ifdef DEBUG
all: $(debug_dirs) node_g
endif


node: $(builddir)/node
	ln -fs $< $@

node_g: $(builddir)/node_g
	ln -fs $< $@



$(dirs) $(debug_dirs):
	mkdir -p $@


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


# node executable

$(builddir)/node: $(node_release_objects) $(libev_release_objects) \
		$(libeio_release_objects) $(http_parser_release_objects) \
		$(cares_release_objects) $(libv8)
	$(CXX) -o $@ $^ $(release_LINKFLAGS) $(node_LINKFLAGS) $(OPENSSL_LINKFLAGS)

$(builddir)/node_g: $(node_debug_objects) $(libev_debug_objects) \
		$(libeio_debug_objects) $(http_parser_debug_objects) \
		$(cares_debug_objects) $(libv8_g)
	$(CXX) -o $@ $^ $(debug_LINKFLAGS) $(node_LINKFLAGS) $(OPENSSL_LINKFLAGS)



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
$(libv8):
	python tools/scons/scons.py -C $(builddir) -Y `pwd`/deps/v8 \
		visibility=default mode=release arch=x64 library=static snapshot=on

$(libv8_g):
	python tools/scons/scons.py -C $(builddir) -Y `pwd`/deps/v8 \
		visibility=default mode=debug arch=x64 library=static snapshot=on


# header deps
$(builddir)/release/src/node.o: $(builddir)/release/src/node_config.h
$(builddir)/debug/src/node.o: $(builddir)/debug/src/node_config.h


# TODO install libs
install: all doc
	$(INSTALL) -d -m 755 '$(PREFIX)/bin'
	$(INSTALL) $(builddir)/node '$(PREFIX)/bin'
	$(INSTALL) -d -m 755 '$(PREFIX)/share/man/man1/'
	$(INSTALL) -d -m 755 '$(PREFIX)/lib/node/wafadmin/Tools'
	$(INSTALL) tools/wafadmin/*.py '$(PREFIX)/lib/node/wafadmin'
	$(INSTALL) tools/wafadmin/Tools/*.py '$(PREFIX)/lib/node/wafadmin/Tools'
	$(INSTALL) doc/node.1 '$(PREFIX)/share/man/man1/'



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

# http://rtomayko.github.com/ronn
# gem install ronn
doc: doc/node.1 doc/api.html doc/index.html doc/changelog.html

## HACK to give the ronn-generated page a TOC
doc/api.html: $(builddir)/node  doc/api.markdown doc/api_header.html doc/api_footer.html
	build/node tools/ronnjs/bin/ronn.js --fragment doc/api.markdown \
	| sed "s/<h2>\(.*\)<\/h2>/<h2 id=\"\1\">\1<\/h2>/g" \
	| cat doc/api_header.html - doc/api_footer.html > doc/api.html

doc/changelog.html: ChangeLog doc/changelog_header.html doc/changelog_footer.html
	cat doc/changelog_header.html ChangeLog doc/changelog_footer.html > doc/changelog.html

doc/node.1: $(builddir)/node doc/api.markdown all
	$(builddir)/node tools/ronnjs/bin/ronn.js --roff doc/api.markdown > doc/node.1

website-upload: doc
	scp doc/* ryan@nodejs.org:~/web/nodejs.org/

docclean:
	@-rm -f doc/node.1 doc/api.html doc/changelog.html

clean:
	-rm -f node node_g $(builddir)/node $(builddir)/node_g
	-find $(builddir) -name "*.o" | xargs rm -f
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


.PHONY: bench clean docclean dist distclean check uninstall install all test test-all website-upload
