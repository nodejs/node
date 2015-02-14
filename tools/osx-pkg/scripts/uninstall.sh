#!/bin/sh
DIR_PREFIX=/usr/local
if [ -f /tmp/iojs-run-uninstall ]; then
  rm -f $DIR_PREFIX/bin/iojs
  rm -f $DIR_PREFIX/bin/npm
  if [ -L $DIR_PREFIX/bin/node ]; then
    rm -f $DIR_PREFIX/bin/node
  fi
  rm -rf $DIR_PREFIX/include/node
  rm -f $DIR_PREFIX/lib/dtrace/node.d
  rm -f $DIR_PREFIX/share/man/man1/iojs.1
  rm -f $DIR_PREFIX/share/systemtap/tapset/node.stp

  rm -f /tmp/iojs-run-uninstall
fi
