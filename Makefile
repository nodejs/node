WAF=python tools/waf-light

web_root = ryan@nodejs.org:~/web/nodejs.org/

#
# Because we recursively call make from waf we need to make sure that we are
# using the correct make. Not all makes are GNU Make, but this likely only
# works with gnu make. To deal with this we remember how the user invoked us
# via a make builtin variable and use that in all subsequent operations
#
export NODE_MAKE := $(MAKE)

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

test-uv: all
	NODE_USE_UV=1 python tools/test.py \
		simple/test-assert \
		simple/test-buffer \
		simple/test-c-ares \
		simple/test-chdir \
		simple/test-delayed-require \
		simple/test-eio-race2 \
		simple/test-eio-race4 \
		simple/test-event-emitter-add-listeners \
		simple/test-event-emitter-modify-in-emit \
		simple/test-event-emitter-num-args \
		simple/test-event-emitter-once \
		simple/test-event-emitter-remove-all-listeners \
		simple/test-event-emitter-remove-listeners \
		simple/test-exception-handler \
		simple/test-exception-handler2 \
		simple/test-exception-handler \
		simple/test-file-read-noexist \
		simple/test-file-write-stream \
		simple/test-fs-fsync \
		simple/test-fs-open \
		simple/test-fs-readfile-empty \
		simple/test-fs-read-file-sync \
		simple/test-fs-read-file-sync-hostname \
		simple/test-fs-sir-writes-alot \
		simple/test-fs-write \
		simple/test-fs-write-buffer \
		simple/test-fs-write-file \
		simple/test-fs-write-file-buffer \
		simple/test-fs-write-stream \
		simple/test-fs-write-stream-end \
		simple/test-fs-write-sync \
		simple/test-global \
		simple/test-http \
		simple/test-http-1.0 \
		simple/test-http-allow-req-after-204-res \
		simple/test-http-blank-header \
		simple/test-http-buffer-sanity \
		simple/test-http-cat \
		simple/test-http-chunked \
		simple/test-http-client-race \
		simple/test-http-client-race-2 \
		simple/test-http-client-upload \
		simple/test-http-client-upload-buf \
		simple/test-http-contentLength0 \
		simple/test-http-dns-fail \
		simple/test-http-eof-on-connect \
		simple/test-http-exceptions \
		simple/test-http-expect-continue \
		simple/test-http-head-request \
		simple/test-http-head-response-has-no-body \
		simple/test-http-keep-alive \
		simple/test-http-keep-alive-close-on-header \
		simple/test-http-malformed-request \
		simple/test-http-parser \
		simple/test-http-proxy \
		simple/test-http-server \
		simple/test-http-server-multiheaders \
		simple/test-http-set-cookies \
		simple/test-http-set-timeout \
		simple/test-http-set-trailers \
		simple/test-http-upgrade-client \
		simple/test-http-upgrade-client2 \
		simple/test-http-upgrade-server \
		simple/test-http-upgrade-server2 \
		simple/test-http-wget \
		simple/test-http-write-empty-string \
		simple/test-http-wget \
		simple/test-mkdir-rmdir \
		simple/test-net-binary \
		simple/test-net-can-reset-timeout \
		simple/test-net-create-connection \
		simple/test-net-eaddrinuse \
		simple/test-net-isip \
		simple/test-net-keepalive \
		simple/test-net-pingpong \
		simple/test-net-reconnect \
		simple/test-net-remote-address-port \
		simple/test-net-server-try-ports \
		simple/test-net-server-stream \
		simple/test-next-tick \
		simple/test-next-tick-errors \
		simple/test-next-tick-ordering \
		simple/test-next-tick-ordering2 \
		simple/test-path \
		simple/test-pump-file2tcp \
		simple/test-pump-file2tcp-noexist \
		simple/test-punycode \
		simple/test-querystring \
		simple/test-readdir \
		simple/test-readdouble \
		simple/test-readfloat \
		simple/test-readint \
		simple/test-readuint \
		simple/test-regress-GH-819 \
		simple/test-regress-GH-897 \
		simple/test-regression-object-prototype \
		simple/test-require-cache \
		simple/test-require-cache-without-stat \
		simple/test-require-exceptions \
		simple/test-require-resolve \
		simple/test-script-context \
		simple/test-script-new \
		simple/test-script-static-context \
		simple/test-script-static-new \
		simple/test-script-static-this \
		simple/test-script-this \
		simple/test-stream-pipe-cleanup \
		simple/test-stream-pipe-error-handling \
		simple/test-stream-pipe-event \
		simple/test-stream-pipe-multi \
		simple/test-string-decoder \
		simple/test-sys \
		simple/test-tcp-wrap \
		simple/test-tcp-wrap-connect \
		simple/test-tcp-wrap-listen \
		simple/test-timers-linked-list \
		simple/test-tty-stdout-end \
		simple/test-url \
		simple/test-utf8-scripts \
		simple/test-vm-create-context-circular-reference \
		simple/test-writedouble \
		simple/test-writefloat \
		simple/test-writeint \
		simple/test-writeuint \
		simple/test-zerolengthbufferbug \
		pummel/test-http-client-reconnect-bug \
		pummel/test-http-upload-timeout \
		pummel/test-net-pause \
		pummel/test-net-pingpong-delay \
		pummel/test-net-timeout \
		pummel/test-timers \
		pummel/test-timer-wrap \
		pummel/test-timer-wrap2 \
		pummel/test-vm-memleak \
		internet/test-dns \


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
