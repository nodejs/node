# Our includes come from the OpenSSL build-tree we're in
INCLUDES		= -I$(top_builddir)/../../include

bin_PROGRAMS		= tunala

tunala_SOURCES		= tunala.c buffer.c cb.c ip.c sm.c breakage.c
tunala_LDADD		= -L$(top_builddir)/../.. -lssl -lcrypto
