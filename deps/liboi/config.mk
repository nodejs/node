# Define EVDIR=/foo/bar if your libev header and library files are in
# /foo/bar/include and /foo/bar/lib directories.
EVDIR=$(HOME)/local/libev

# Define GNUTLSDIR=/foo/bar if your gnutls header and library files are in
# /foo/bar/include and /foo/bar/lib directories.
#GNUTLSDIR=/usr
#
#
# Define NO_PREAD if you have a problem with pread() system call (e.g.
# cygwin.dll before v1.5.22).
#
#
# Define NO_SENDFILE if you have a problem with the sendfile() system call
#
#

uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
uname_M := $(shell sh -c 'uname -m 2>/dev/null || echo not')
uname_O := $(shell sh -c 'uname -o 2>/dev/null || echo not')
uname_R := $(shell sh -c 'uname -r 2>/dev/null || echo not')
uname_P := $(shell sh -c 'uname -p 2>/dev/null || echo not')

# CFLAGS and LDFLAGS are for the users to override from the command line.
CFLAGS	= -g
LDFLAGS	= 

PREFIX = $(HOME)/local/liboi

CC = gcc
AR = ar
RM = rm -f
RANLIB = ranlib
