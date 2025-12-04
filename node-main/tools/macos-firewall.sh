#!/bin/sh
# Script that adds rules to Mac OS X Socket Firewall to avoid
# popups asking to accept incoming network connections when
# running tests.
SFW="/usr/libexec/ApplicationFirewall/socketfilterfw"
TOOLSDIR="$(dirname "$0")"
TOOLSDIR="$(cd "$TOOLSDIR" && pwd)"
ROOTDIR="$(cd "$TOOLSDIR/.." && pwd)"
OUTDIR="$TOOLSDIR/../out"
# Using cd and pwd here so that the path used for socketfilterfw does not
# contain a '..', which seems to cause the rules to be incorrectly added
# and they are not removed when this script is re-run. Instead the new
# rules are simply appended. By using pwd we can get the full path
# without '..' and things work as expected.
OUTDIR="$(cd "$OUTDIR" && pwd)"
NODE_RELEASE="$OUTDIR/Release/node"
NODE_DEBUG="$OUTDIR/Debug/node"
NODE_LINK="$ROOTDIR/node"
CCTEST_RELEASE="$OUTDIR/Release/cctest"
CCTEST_DEBUG="$OUTDIR/Debug/cctest"
OPENSSL_CLI_RELEASE="$OUTDIR/Release/openssl-cli"
OPENSSL_CLI_DEBUG="$OUTDIR/Debug/openssl-cli"

add_and_unblock () {
  if [ -e "$1" ]
  then
    echo Processing "$1"
    $SFW --remove "$1" >/dev/null
    $SFW --add "$1"
    $SFW --unblock "$1"
  fi
}

if [ -f $SFW ];
then
  add_and_unblock "$NODE_DEBUG"
  add_and_unblock "$NODE_RELEASE"
  add_and_unblock "$NODE_LINK"
  add_and_unblock "$CCTEST_DEBUG"
  add_and_unblock "$CCTEST_RELEASE"
  add_and_unblock "$OPENSSL_CLI_DEBUG"
  add_and_unblock "$OPENSSL_CLI_RELEASE"
else
  echo "SocketFirewall not found in location: $SFW"
fi
