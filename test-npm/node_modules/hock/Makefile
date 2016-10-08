MOCHA_OPTS=-t 4000 test/*-test.js
REPORTER = spec

check: test

test: test-unit

test-unit:
	@NODE_ENV=test MOCK=on ./node_modules/.bin/mocha \
	    --reporter $(REPORTER) \
		$(MOCHA_OPTS)

.PHONY: test