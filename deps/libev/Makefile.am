AUTOMAKE_OPTIONS = foreign no-dependencies

VERSION_INFO = 3:0

EXTRA_DIST = LICENSE Changes libev.m4 autogen.sh \
	     ev_vars.h ev_wrap.h \
	     ev_epoll.c ev_select.c ev_poll.c ev_kqueue.c ev_port.c ev_win32.c \
	     ev.3 ev.pod

man_MANS = ev.3

include_HEADERS = ev.h ev++.h event.h

lib_LTLIBRARIES = libev.la

libev_la_SOURCES = ev.c event.c
libev_la_LDFLAGS = -version-info $(VERSION_INFO)

