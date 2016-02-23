#!/bin/sh
/bin/rm -f mttest
purify cc -DSOLARIS -I../../include -g mttest.c -o mttest -L../.. -lthread  -lssl -lcrypto -lnsl -lsocket

