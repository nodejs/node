#!/bin/sh

if [ -z "$DIR_PREFIX" ]; then
  DIR_PREFIX=/usr/local
fi

if [ -f /tmp/nodejs-run-uninstall ]; then
  rm -f $DIR_PREFIX/bin/node
  rm -f $DIR_PREFIX/bin/npm
  rm -rf $DIR_PREFIX/include/node
  rm -f $DIR_PREFIX/lib/dtrace/node.d
  rm -f $DIR_PREFIX/share/man/man1/node.1
  rm -f $DIR_PREFIX/share/systemtap/tapset/node.stp
  rm -f $DIR_PREFIX/share/doc/node/gdbinit

  rm -rf $DIR_PREFIX/lib/node_modules/npm
  if [ ! "$(ls -A $DIR_PREFIX/lib/node_modules 2> /dev/null)" ]; then
    rm -rf $DIR_PREFIX/lib/node_modules
  fi

  rm -f /tmp/nodejs-run-uninstall
fi
