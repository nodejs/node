#!/bin/sh
/bin/rm -f mttest
cc -DSOLARIS -I../../include -g mttest.c -o mttest -L../.. -lthread  -lssl -lcrypto -lnsl -lsocket

