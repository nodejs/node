#!/bin/sh
/bin/rm -f mttest
cc -p -DSOLARIS -I../../include -g mttest.c -o mttest -L/usr/lib/libc -ldl -L../.. -lthread  -lssl -lcrypto -lnsl -lsocket

