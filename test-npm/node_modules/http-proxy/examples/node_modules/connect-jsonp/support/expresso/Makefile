
PREFIX ?= /usr/local
BIN = bin/expresso
JSCOV = deps/jscoverage/node-jscoverage
DOCS = docs/index.md
HTMLDOCS = $(DOCS:.md=.html)

test: $(BIN)
	@./$(BIN) -I lib --growl $(TEST_FLAGS) test/*.test.js

test-cov:
	@./$(BIN) -I lib --cov $(TEST_FLAGS) test/*.test.js

test-serial:
	@./$(BIN) --serial -I lib $(TEST_FLAGS) test/serial/*.test.js

install: install-jscov install-expresso

uninstall:
	rm -f $(PREFIX)/bin/expresso
	rm -f $(PREFIX)/bin/node-jscoverage

install-jscov: $(JSCOV)
	install $(JSCOV) $(PREFIX)/bin

install-expresso:
	install $(BIN) $(PREFIX)/bin

$(JSCOV):
	cd deps/jscoverage && ./configure && make && mv jscoverage node-jscoverage

clean:
	@cd deps/jscoverage && git clean -fd

docs: docs/api.html $(HTMLDOCS)

%.html: %.md
	@echo "... $< > $@"
	@ronn -5 --pipe --fragment $< \
		| cat docs/layout/head.html - docs/layout/foot.html \
		> $@

docs/api.html: bin/expresso
	dox \
		--title "Expresso" \
		--ribbon "http://github.com/visionmedia/expresso" \
		--desc "Insanely fast TDD framework for [node](http://nodejs.org) featuring code coverage reporting." \
		$< > $@

docclean:
	rm -f docs/*.html

.PHONY: test test-cov install uninstall install-expresso install-jscov clean docs docclean