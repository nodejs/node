#!/bin/sh

# This script tries to follow the "GNU way" w.r.t. the autobits.
# This does of course generate a number of irritating files.
# Try to get over it (I am getting there myself).

# This should generate any missing crud, and then run autoconf which should turn
# configure.in into a "./configure" script and "Makefile.am" into a
# "Makefile.in". Then running "./configure" should turn "Makefile.in" into
# "Makefile" and should generate the config.h containing your systems various
# settings. I know ... what a hassle ...

# Also, sometimes these autobits things generate bizarre output (looking like
# errors). So I direct everything "elsewhere" ...

(aclocal
autoheader
libtoolize --copy --force
automake --foreign --add-missing --copy
autoconf) 1> /dev/null 2>&1

# Move the "no-autotools" Makefile out of the way
if test ! -f Makefile.plain; then
	mv Makefile Makefile.plain
fi
