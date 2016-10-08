REPORTER = spec
test:
	@NODE_ENV=test ./node_modules/.bin/mocha -b --reporter $(REPORTER) --check-leaks

lint:
	./node_modules/.bin/jshint ./test ./index.js

test-cov:
	$(MAKE) lint
	@NODE_ENV=test ./node_modules/.bin/istanbul cover \
	./node_modules/mocha/bin/_mocha -- -R spec --check-leaks

test-coveralls:
	echo TRAVIS_JOB_ID $(TRAVIS_JOB_ID)
	$(MAKE) test
	@NODE_ENV=test ./node_modules/.bin/istanbul cover \
	./node_modules/mocha/bin/_mocha --check-leaks --report lcovonly -- -R spec && \
		cat ./coverage/lcov.info | ./node_modules/coveralls/bin/coveralls.js || true

.PHONY: test
