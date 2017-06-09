#!/bin/sh

BRANCH=`git rev-parse --abbrev-ref HEAD`

./Configure $@ no-symlinks
make files
util/mk1mf.pl OUT=out.$BRANCH TMP=tmp.$BRANCH INC=inc.$BRANCH copy > makefile.$BRANCH
make -f makefile.$BRANCH init
