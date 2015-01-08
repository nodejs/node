#!/bin/sh
basedir=`dirname "$0"`

case `uname` in
    *CYGWIN*) basedir=`cygpath -w "$basedir"`;;
esac

if [ -x "$basedir/node" ]; then
  "$basedir/node"  "$basedir/../rimraf/bin.js" "$@"
  ret=$?
else 
  node  "$basedir/../rimraf/bin.js" "$@"
  ret=$?
fi
exit $ret
