#!/bin/sh
basedir=$(dirname "$(echo "$0" | sed -e 's,\\,/,g')")

case `uname` in
    *CYGWIN*) basedir=`cygpath -w "$basedir"`;;
esac

if [ -x "$basedir/node" ]; then
  exec "$basedir/node"  "$basedir/node_modules/corepack/dist/npx.js" "$@"
else
  exec node  "$basedir/node_modules/corepack/dist/npx.js" "$@"
fi
