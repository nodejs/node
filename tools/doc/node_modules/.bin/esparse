#!/bin/sh
basedir=`dirname "$0"`

case `uname` in
    *CYGWIN*) basedir=`cygpath -w "$basedir"`;;
esac

if [ -x "$basedir/node" ]; then
  "$basedir/node"  "$basedir/../esprima/bin/esparse.js" "$@"
  ret=$?
else 
  node  "$basedir/../esprima/bin/esparse.js" "$@"
  ret=$?
fi
exit $ret
