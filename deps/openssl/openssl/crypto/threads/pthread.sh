#!/bin/sh
#
# build using pthreads
#
# http://www.mit.edu:8001/people/proven/pthreads.html
#
/bin/rm -f mttest
pgcc -DPTHREADS -I../../include -g mttest.c -o mttest -L../.. -lssl -lcrypto 

