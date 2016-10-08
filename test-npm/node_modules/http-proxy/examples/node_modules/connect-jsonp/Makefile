TEST = support/expresso/bin/expresso
TESTS ?= test/*.js
SRC = $(shell find lib -type f -name "*.js")

test:
	@NODE_ENV=test ./$(TEST) \
		-I lib \
		-I support \
		$(TEST_FLAGS) $(TESTS)
		
.PHONY: test