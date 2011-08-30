BUILDTYPE ?= Release

all: out/Makefile
	tools/gyp_node -f make
	$(MAKE) -C out BUILDTYPE=$(BUILDTYPE)
	-ln -fs out/Release/node node
	-ln -fs out/Debug/node node_g

out/Release/node: all

out/Makefile: node.gyp deps/uv/uv.gyp

clean:
	rm -rf out

distclean:
	rm -rf out

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

UVTEST += simple/test-assert
UVTEST += simple/test-buffer
UVTEST += simple/test-c-ares
UVTEST += simple/test-chdir
UVTEST += simple/test-delayed-require
UVTEST += simple/test-eio-race2
UVTEST += simple/test-eio-race4
UVTEST += simple/test-event-emitter-add-listeners
UVTEST += simple/test-event-emitter-modify-in-emit
UVTEST += simple/test-event-emitter-num-args
UVTEST += simple/test-event-emitter-once
UVTEST += simple/test-event-emitter-remove-all-listeners
UVTEST += simple/test-event-emitter-remove-listeners
UVTEST += simple/test-exception-handler
UVTEST += simple/test-exception-handler2
UVTEST += simple/test-exception-handler
UVTEST += simple/test-executable-path
UVTEST += simple/test-file-read-noexist
UVTEST += simple/test-file-write-stream
UVTEST += simple/test-fs-fsync
UVTEST += simple/test-fs-open
UVTEST += simple/test-fs-readfile-empty
UVTEST += simple/test-fs-read-file-sync
UVTEST += simple/test-fs-read-file-sync-hostname
UVTEST += simple/test-fs-sir-writes-alot
UVTEST += simple/test-fs-write
UVTEST += simple/test-fs-write-buffer
UVTEST += simple/test-fs-write-file
UVTEST += simple/test-fs-write-file-buffer
UVTEST += simple/test-fs-write-stream
UVTEST += simple/test-fs-write-stream-end
UVTEST += simple/test-fs-write-sync
UVTEST += simple/test-global
UVTEST += simple/test-http
UVTEST += simple/test-http-1.0
UVTEST += simple/test-http-abort-client
UVTEST += simple/test-http-allow-req-after-204-res
UVTEST += simple/test-http-blank-header
UVTEST += simple/test-http-buffer-sanity
UVTEST += simple/test-http-cat
UVTEST += simple/test-http-chunked
UVTEST += simple/test-http-client-abort
UVTEST += simple/test-http-client-parse-error
UVTEST += simple/test-http-client-race
UVTEST += simple/test-http-client-race-2
UVTEST += simple/test-http-client-upload
UVTEST += simple/test-http-client-upload-buf
UVTEST += simple/test-http-contentLength0
UVTEST += simple/test-http-default-encoding
UVTEST += simple/test-http-dns-fail
UVTEST += simple/test-http-eof-on-connect
UVTEST += simple/test-http-exceptions
UVTEST += simple/test-http-expect-continue
UVTEST += simple/test-http-extra-response
UVTEST += simple/test-http-head-request
UVTEST += simple/test-http-head-response-has-no-body
UVTEST += simple/test-http-keep-alive
UVTEST += simple/test-http-keep-alive-close-on-header
UVTEST += simple/test-http-malformed-request
UVTEST += simple/test-http-many-keep-alive-connections
UVTEST += simple/test-http-mutable-headers
UVTEST += simple/test-http-parser
UVTEST += simple/test-http-proxy
UVTEST += simple/test-http-request-end
UVTEST += simple/test-http-response-close
UVTEST += simple/test-http-response-readable
UVTEST += simple/test-http-unix-socket
UVTEST += simple/test-http-server
UVTEST += simple/test-http-server-multiheaders
UVTEST += simple/test-http-set-cookies
UVTEST += simple/test-http-set-timeout
UVTEST += simple/test-http-set-trailers
UVTEST += simple/test-http-upgrade-agent
UVTEST += simple/test-http-upgrade-client
UVTEST += simple/test-http-upgrade-client2
UVTEST += simple/test-http-upgrade-server
UVTEST += simple/test-http-upgrade-server2
UVTEST += simple/test-http-wget
UVTEST += simple/test-http-write-empty-string
UVTEST += simple/test-http-wget
UVTEST += simple/test-mkdir-rmdir
UVTEST += simple/test-net-binary
UVTEST += simple/test-net-pingpong
UVTEST += simple/test-net-can-reset-timeout
UVTEST += simple/test-net-connect-buffer
UVTEST += simple/test-net-connect-timeout
UVTEST += simple/test-net-create-connection
UVTEST += simple/test-net-eaddrinuse
UVTEST += simple/test-net-isip
UVTEST += simple/test-net-keepalive
UVTEST += simple/test-net-pingpong
UVTEST += simple/test-net-reconnect
UVTEST += simple/test-net-remote-address-port
UVTEST += simple/test-net-server-bind
UVTEST += simple/test-net-server-max-connections
UVTEST += simple/test-net-server-try-ports
UVTEST += simple/test-net-stream
UVTEST += simple/test-net-socket-timeout
UVTEST += simple/test-next-tick
UVTEST += simple/test-next-tick-doesnt-hang
UVTEST += simple/test-next-tick-errors
UVTEST += simple/test-next-tick-ordering
UVTEST += simple/test-next-tick-ordering2
UVTEST += simple/test-next-tick-starvation
UVTEST += simple/test-module-load-list
UVTEST += simple/test-path
UVTEST += simple/test-pipe-stream
UVTEST += simple/test-pump-file2tcp
UVTEST += simple/test-pump-file2tcp-noexist
UVTEST += simple/test-punycode
UVTEST += simple/test-querystring
UVTEST += simple/test-readdir
UVTEST += simple/test-readdouble
UVTEST += simple/test-readfloat
UVTEST += simple/test-readint
UVTEST += simple/test-readuint
UVTEST += simple/test-regress-GH-819
UVTEST += simple/test-regress-GH-897
UVTEST += simple/test-regression-object-prototype
UVTEST += simple/test-require-cache
UVTEST += simple/test-require-cache-without-stat
UVTEST += simple/test-require-exceptions
UVTEST += simple/test-require-resolve
UVTEST += simple/test-script-context
UVTEST += simple/test-script-new
UVTEST += simple/test-script-static-context
UVTEST += simple/test-script-static-new
UVTEST += simple/test-script-static-this
UVTEST += simple/test-script-this
UVTEST += simple/test-stream-pipe-cleanup
UVTEST += simple/test-stream-pipe-error-handling
UVTEST += simple/test-stream-pipe-event
UVTEST += simple/test-stream-pipe-multi
UVTEST += simple/test-string-decoder
UVTEST += simple/test-sys
UVTEST += simple/test-tcp-wrap
UVTEST += simple/test-tcp-wrap-connect
UVTEST += simple/test-tcp-wrap-listen
UVTEST += simple/test-timers-linked-list
UVTEST += simple/test-tty-stdout-end
UVTEST += simple/test-url
UVTEST += simple/test-utf8-scripts
UVTEST += simple/test-vm-create-context-circular-reference
UVTEST += simple/test-writedouble
UVTEST += simple/test-writefloat
UVTEST += simple/test-writeint
UVTEST += simple/test-writeuint
UVTEST += simple/test-zerolengthbufferbug
UVTEST += pummel/test-http-client-reconnect-bug
UVTEST += pummel/test-http-upload-timeout
UVTEST += pummel/test-net-many-clients
UVTEST += pummel/test-net-pause
UVTEST += pummel/test-net-pingpong-delay
UVTEST += pummel/test-net-timeout
UVTEST += pummel/test-timers
UVTEST += pummel/test-timer-wrap
UVTEST += pummel/test-timer-wrap2
UVTEST += pummel/test-vm-memleak
UVTEST += internet/test-dns
UVTEST += simple/test-tls-client-abort
UVTEST += simple/test-tls-client-verify
UVTEST += simple/test-tls-connect
#UVTEST += simple/test-tls-ext-key-usage # broken
UVTEST += simple/test-tls-junk-closes-server
UVTEST += simple/test-tls-npn-server-client
UVTEST += simple/test-tls-request-timeout
#UVTEST += simple/test-tls-securepair-client # broken
UVTEST += simple/test-tls-securepair-server 
#UVTEST += simple/test-tls-server-verify # broken
UVTEST += simple/test-tls-set-encoding

# child_process
UVTEST += simple/test-child-process-exit-code
UVTEST += simple/test-child-process-buffering
UVTEST += simple/test-child-process-exec-cwd
UVTEST += simple/test-child-process-cwd
UVTEST += simple/test-child-process-env
UVTEST += simple/test-child-process-stdin
UVTEST += simple/test-child-process-ipc
UVTEST += simple/test-child-process-deprecated-api


test-uv: all
	NODE_USE_UV=1 python tools/test.py $(UVTEST)

test-uv-debug: all
	NODE_USE_UV=1 python tools/test.py --mode=debug $(UVTEST)


apidoc_sources = $(wildcard doc/api/*.markdown)
apidocs = $(addprefix out/,$(apidoc_sources:.markdown=.html))

apidoc_dirs = out/doc out/doc/api/ out/doc/api/assets

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
	out/doc/pipe.css

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
	scp -r out/doc/* $(web_root)

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

.PHONY: lint cpplint jslint bench clean docopen docclean doc dist distclean check uninstall install all program staticlib dynamiclib test test-all website-upload
