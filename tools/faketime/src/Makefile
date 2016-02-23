#
# Notes:
#
#   * Compilation Defines:
#
#     FAKE_STAT
#         - Enables time faking also for files' timestamps.
#
#     NO_ATFILE
#         - Disables support for the fstatat() group of functions
#
#     PTHREAD
#         - Define this to enable multithreading support.
#
#     PTHREAD_SINGLETHREADED_TIME
#         - Define this if you want to single-thread time() ... there ARE
#           possibile caching side-effects in a multithreaded environment
#           without this, but the performance impact may require you to
#           try it unsynchronized.
#
#     FAKE_INTERNAL_CALLS
#         - Also intercept libc internal __functions, e.g. not just time(),
#           but also __time(). Enhances compatibility with applications
#           that make use of low-level system calls, such as Java Virtual
#           Machines.
#
# 	  FAKE_SLEEP
# 	      - Also intercept sleep(), nanosleep(), usleep(), alarm(), [p]poll()
#
#	  FAKE_TIMERS
#	      - Also intercept timer_settime() and timer_gettime()
#
#	  MULTI_ARCH
#	  	  - If MULTI_ARCH is set, the faketime wrapper program will put a literal
#	  	    $LIB into the LD_PRELOAD environment variable it creates, which makes
#	  	    ld automatically choose the correct library version to use for the
#	  	    target binary. Use for Linux platforms with Multi-Arch support only!
#
#   * Compilation addition: second libMT target added for building the pthread-
#     enabled library as a separate library
#
#   * Compilation switch change: previous versions compiled using '-nostartfiles'
#     This is no longer the case since there is a 'startup' constructor for the library
#     which is used to activate the start-at times when specified. This also initializes
#     the dynamic disabling of the FAKE_STAT calls.
#
# By default, libfaketime will be compiled for your system's default architecture.
# To build 32-bit libraries and binaries, add -m32 to CFLAGS and LDFLAGS.
#
# Change PREFIX to where you want libfaketime (libraries and wrapper binary) installed.
# LIBDIRNAME is relative to PREFIX. The default is to install into $PREFIX/lib/faketime,
# but you can set LIBDIRNAME to, e.g., /lib64 if you want to install it elsewhere.
# LIBDIRNAME has been introduced to support MultiLib systems. Please do not change the 
# default value on MultiArch systems. 
#
# For testing in the current directory without installation, try make PREFIX= LIBDIRNAME='.'

CC ?= gcc
INSTALL ?= install

PREFIX ?= /usr/local
LIBDIRNAME ?= /lib/faketime
PLATFORM ?=$(shell uname)

CFLAGS += -std=gnu99 -Wall -Wextra -Werror -DFAKE_STAT -DFAKE_SLEEP -DFAKE_TIMERS -DFAKE_INTERNAL_CALLS -fPIC -DPREFIX='"'$(PREFIX)'"' -DLIBDIRNAME='"'$(LIBDIRNAME)'"'
ifeq ($(PLATFORM),SunOS)
CFLAGS += -D__EXTENSIONS__ -D_XOPEN_SOURCE=600
endif

LIB_LDFLAGS += -shared

LDFLAGS += -lpthread
ifneq ($(PLATFORM),SunOS)
LDFLAGS += -Wl,--version-script=libfaketime.map
endif

LDADD += -ldl -lm -lrt
BIN_LDFLAGS += -lrt

SRC = libfaketime.c
LIBS_OBJ = libfaketime.o libfaketimeMT.o
BINS = faketime

SONAME = 1
LIBS = libfaketime.so.${SONAME} libfaketimeMT.so.${SONAME}

all: ${LIBS} ${BINS}

libfaketimeMT.o: EXTRA_FLAGS := -DPTHREAD -DPTHREAD_SINGLETHREADED_TIME

${LIBS_OBJ}: libfaketime.c
	${CC} -o $@ -c ${CFLAGS} ${EXTRA_FLAGS} $<

%.so.${SONAME}: %.o libfaketime.map
	${CC} -o $@ -Wl,-soname,$@ ${LDFLAGS} ${LIB_LDFLAGS} $< ${LDADD}

${BINS}: faketime.c
	${CC} -o $@ ${CFLAGS} ${EXTRA_FLAGS} $< ${LDFLAGS} ${BIN_LDFLAGS}

clean:
	@rm -f ${LIBS_OBJ} ${LIBS} ${BINS}

distclean: clean
	@echo

install: ${LIBS} ${BINS}
	@echo
	@echo "Copying the faketime libraries to ${DESTDIR}${PREFIX}${LIBDIRNAME} and the faketime wrapper script to ${DESTDIR}${PREFIX}/bin ..."
	$(INSTALL) -dm0755 "${DESTDIR}${PREFIX}${LIBDIRNAME}/"
	$(INSTALL) -m0644 ${LIBS} "${DESTDIR}${PREFIX}${LIBDIRNAME}/"
	$(INSTALL) -Dm0755 faketime "${DESTDIR}${PREFIX}/bin/faketime"

uninstall:
	for f in ${LIBS}; do rm -f "${DESTDIR}${PREFIX}${LIBDIRNAME}/$$f"; done
	rmdir "${DESTDIR}${PREFIX}${LIBDIRNAME}"
	rm -f "${DESTDIR}${PREFIX}/bin/faketime"

.PHONY: all clean distclean install uninstall
