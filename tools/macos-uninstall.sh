#!/bin/sh

# Remove Node.js installed from tarballs or installer on macOS.
# Referencing issue: https://github.com/nodejs/node/issues/7371
# Using gist from rvagg: https://github.com/rvagg/io.js/blob/fhemberger-refactor-mac-installer-rebase/tools/osx-pkg/scripts/uninstall.sh

if [ -z "$DIR_PREFIX" ]; then
  DIR_PREFIX=/usr/local
fi

rm -f "$DIR_PREFIX/bin/node"
rm -f "$DIR_PREFIX/bin/npm"
rm -rf "$DIR_PREFIX/include/node"
rm -f "$DIR_PREFIX/lib/dtrace/node.d"
rm -f "$DIR_PREFIX/share/man/man1/node.1"
rm -f "$DIR_PREFIX/share/systemtap/tapset/node.stp"
rm -f "$DIR_PREFIX/share/doc/node/gdbinit"
rm -rf "$DIR_PREFIX/lib/node_modules/npm"
rm -f "$DIR_PREFIX/lib/node_modules/.DS_Store"

if [ ! "$(ls -A "$DIR_PREFIX/lib/node_modules" 2> /dev/null)" ]; then
    rm -df "$DIR_PREFIX/lib/node_modules"
fi
