#!/bin/bash
# Script that adds rules to Mac OS X Socket Firewall to avoid
# popups asking to accept incoming network connections when
# running tests.
SFW="/usr/libexec/ApplicationFirewall/socketfilterfw"
TOOLSDIR="`dirname \"$0\"`"
TOOLSDIR="`( cd \"$TOOLSDIR\" && pwd) `"
ROOTDIR="`( cd \"$TOOLSDIR/..\" && pwd) `"
OUTDIR="$TOOLSDIR/../out"
# Using cd and pwd here so that the path used for socketfilterfw does not
# contain a '..', which seems to cause the rules to be incorrectly added
# and they are not removed when this script is re-run. Instead the new
# rules are simply appended. By using pwd we can get the full path
# without '..' and things work as expected.
OUTDIR="`( cd \"$OUTDIR\" && pwd) `"
NODE_RELEASE="$OUTDIR/Release/node"
NODE_DEBUG="$OUTDIR/Debug/node"
NODE_LINK="$ROOTDIR/node"
CCTEST_RELEASE="$OUTDIR/Release/cctest"
CCTEST_DEBUG="$OUTDIR/Debug/cctest"
OPENSSL_CLI_RELEASE="$OUTDIR/Release/openssl-cli"
OPENSSL_CLI_DEBUG="$OUTDIR/Debug/openssl-cli"

if [ -f $SFW ];
then
  # Duplicating these commands on purpose as the symbolic link node might be
  # linked to either out/Debug/node or out/Release/node depending on the
  # BUILDTYPE.
  $SFW --remove "$NODE_DEBUG"
  $SFW --remove "$NODE_DEBUG"
  $SFW --remove "$NODE_RELEASE"
  $SFW --remove "$NODE_RELEASE"
  $SFW --remove "$NODE_LINK"
  $SFW --remove "$CCTEST_DEBUG"
  $SFW --remove "$CCTEST_RELEASE"
  $SFW --remove "$OPENSSL_CLI_DEBUG"
  $SFW --remove "$OPENSSL_CLI_RELEASE"

  $SFW --add "$NODE_DEBUG"
  $SFW --add "$NODE_RELEASE"
  $SFW --add "$NODE_LINK"
  $SFW --add "$CCTEST_DEBUG"
  $SFW --add "$CCTEST_RELEASE"
  $SFW --add "$OPENSSL_CLI_DEBUG"
  $SFW --add "$OPENSSL_CLI_RELEASE"

  $SFW --unblock "$NODE_DEBUG"
  $SFW --unblock "$NODE_RELEASE"
  $SFW --unblock "$NODE_LINK"
  $SFW --unblock "$CCTEST_DEBUG"
  $SFW --unblock "$CCTEST_RELEASE"
  $SFW --unblock "$OPENSSL_CLI_DEBUG"
  $SFW --unblock "$OPENSSL_CLI_RELEASE"
else
  echo "SocketFirewall not found in location: $SFW"
fi
