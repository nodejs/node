# Copyright Joyent, Inc. and other Node contributors. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

OS ?= $(shell sh -c 'uname -s | tr "[A-Z]" "[a-z]"')

OBJS= \
		src/ares_cancel.o \
		src/ares__close_sockets.o \
		src/ares_create_query.o \
		src/ares_data.o \
		src/ares_destroy.o \
		src/ares_expand_name.o \
		src/ares_expand_string.o \
		src/ares_fds.o \
		src/ares_free_hostent.o \
		src/ares_free_string.o \
		src/ares_gethostbyaddr.o \
		src/ares_gethostbyname.o \
		src/ares__get_hostent.o \
		src/ares_getnameinfo.o \
		src/ares_getopt.o \
		src/ares_getsock.o \
		src/ares_init.o \
		src/ares_library_init.o \
		src/ares_llist.o \
		src/ares_mkquery.o \
		src/ares_nowarn.o \
		src/ares_options.o \
		src/ares_parse_aaaa_reply.o \
		src/ares_parse_a_reply.o \
		src/ares_parse_mx_reply.o \
		src/ares_parse_naptr_reply.o \
		src/ares_parse_ns_reply.o \
		src/ares_parse_ptr_reply.o \
		src/ares_parse_soa_reply.o \
		src/ares_parse_srv_reply.o \
		src/ares_parse_txt_reply.o \
		src/ares_process.o \
		src/ares_query.o \
		src/ares__read_line.o \
		src/ares_search.o \
		src/ares_send.o \
		src/ares_strcasecmp.o \
		src/ares_strdup.o \
		src/ares_strerror.o \
		src/ares_timeout.o \
		src/ares__timeval.o \
		src/ares_version.o \
		src/ares_writev.o \
		src/bitncmp.o \
		src/inet_net_pton.o \
		src/inet_ntop.o \

CFLAGS += -I. -I$(SRCDIR)/include -DHAVE_CONFIG_H

ARES_CONFIG_OS = $(OS)
SOEXT = so

# if on windows
ifneq (,$(findstring mingw,$(OS)))
ARES_CONFIG_OS = win32
OBJS += src/windows_port.o
OBJS += src/ares_getenv.o
OBJS += src/ares_platform.o

LDFLAGS += -lws2_32.lib -liphlpapi.lib
else # else a posix system
CFLAGS += -g --std=gnu89 -pedantic
CFLAGS += -Wall -Wextra -Wno-unused-parameter
CFLAGS += -D_LARGEFILE_SOURCE
CFLAGS += -D_FILE_OFFSET_BITS=64
endif

ifneq (,$(findstring cygwin,$(OS)))
ARES_CONFIG_OS = cygwin
CFLAGS += -D_GNU_SOURCE
endif

ifeq (dragonflybsd,$(OS))
ARES_CONFIG_OS = freebsd
endif

ifeq (darwin,$(OS))
CFLAGS += -D_DARWIN_USE_64_BIT_INODE=1
LDFLAGS += -dynamiclib -install_name "@rpath/libcares.dylib"
SOEXT = dylib
endif

ifeq (linux,$(OS))
CFLAGS += -D_GNU_SOURCE
endif

ifeq (android,$(OS))
CFLAGS += -D_GNU_SOURCE
endif

ifeq (sunos,$(OS))
LDFLAGS += -lsocket -lnsl
CFLAGS += -D__EXTENSIONS__ -D_XOPEN_SOURCE=500
endif

CFLAGS += -I$(SRCDIR)/config/$(ARES_CONFIG_OS)

ifneq (,$(findstring libcares.$(SOEXT),$(MAKECMDGOALS)))
CFLAGS += -DCARES_BUILDING_LIBRARY
else
CFLAGS += -DCARES_STATICLIB
endif

all: libcares.a

src/.buildstamp:
	mkdir -p $(dir $@)
	touch $@

libcares.a: $(OBJS)
	$(AR) rcs $@ $^

libcares.$(SOEXT): override CFLAGS += -fPIC
libcares.$(SOEXT): $(OBJS:%.o=%.pic.o)
	$(CC) -shared -o $@ $^ $(LDFLAGS)

src/%.o src/%.pic.o:  src/%.c include/ares.h include/ares_version.h \
		include/nameser.h src/.buildstamp \
		$(SRCDIR)/config/$(ARES_CONFIG_OS)/ares_config.h
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	$(RM) -f libcares.a libcares.$(SOEXT) src/*.o src/.buildstamp
