all: .DEFAULT
.DEFAULT:
	@which gmake > /dev/null 2>&1 ||\
		(echo "GMake is required for io.js to build.\
			Install and try again" && exit 1)
	@gmake ${.MAKEFLAGS} ${.TARGETS}

.PHONY: test
