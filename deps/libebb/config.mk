PREFIX = $(HOME)/local/libebb

# libev
EVINC  = $(HOME)/local/libev/include
EVLIB  = $(HOME)/local/libev/lib
EVLIBS = -L${EVLIB} -lev

# GnuTLS, comment if you don't want it (necessary for HTTPS)
GNUTLSLIB   = /usr/lib
GNUTLSINC   = /usr/include
GNUTLSLIBS  = -L${GNUTLSLIB} -lgnutls
GNUTLSFLAGS = -DHAVE_GNUTLS

# includes and libs
INCS = -I${EVINC} -I${GNUTLSINC}
LIBS = ${EVLIBS} ${GNUTLSLIBS} -lefence

# flags
CPPFLAGS = -DVERSION=\"$(VERSION)\" ${GNUTLSFLAGS}
CFLAGS   = -O2 -g -Wall ${INCS} ${CPPFLAGS} -fPIC
LDFLAGS  = -s ${LIBS}
LDOPT    = -shared
SUFFIX   = so
SONAME   = -Wl,-soname,$(OUTPUT_LIB)

# Solaris
#CFLAGS  = -fast ${INCS} -DVERSION=\"$(VERSION)\" -fPIC
#LDFLAGS = ${LIBS}
#SONAME  = 

# Darwin
# LDOPT  = -dynamiclib 
# SUFFIX = dylib
# SONAME = -current_version $(VERSION) -compatibility_version $(VERSION)

# compiler and linker
CC = cc
RANLIB = ranlib
