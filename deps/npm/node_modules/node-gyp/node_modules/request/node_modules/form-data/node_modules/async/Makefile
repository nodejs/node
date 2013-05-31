PACKAGE = asyncjs
NODEJS = $(if $(shell test -f /usr/bin/nodejs && echo "true"),nodejs,node)

BUILDDIR = dist

all: build

build: $(wildcard  lib/*.js)
	mkdir -p $(BUILDDIR)
	uglifyjs lib/async.js > $(BUILDDIR)/async.min.js

test:
	nodeunit test

clean:
	rm -rf $(BUILDDIR)

lint:
	nodelint --config nodelint.cfg lib/async.js

.PHONY: test build all
