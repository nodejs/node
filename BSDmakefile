# pmake might add -J (private)
FLAGS=${.MAKEFLAGS:C/\-J ([0-9]+,?)+//W}

all: .DEFAULT
.DEFAULT:
	@which gmake > /dev/null 2>&1 ||\
		(echo "GMake is required for node.js to build.\
			Install and try again" && exit 1)
	@gmake ${.FLAGS} ${.TARGETS}

.PHONY: test
