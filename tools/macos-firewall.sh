#!/bin/bash
# Script that adds rules to Mac OS X Socket Firewall to avoid
# popups asking to accept incoming network connections when
# running tests.
SFW="/usr/libexec/ApplicationFirewall/socketfilterfw"
TOOLSDIR="`dirname \"$0\"`"cript that adds rules to Mac OS X Socket Firewall to avoid
3
# popups asking to accept incoming network connections when
4
# running tests.
5
SFW="/usr/libexec/ApplicationFirewall/socketfilterfw"
6
TOOLSDIR="`dirname \"$0\"`"
7
TOOLSDIR="`( cd \"$TOOLSDIR\" && pwd) `"
8
ROOTDIR="`( cd \"$TOOLSDIR/..\" && pwd) `"
9
OUTDIR="$TOOLSDIR/../out"
10
# Using cd and pwd here so that the path used for socketfilterfw does not
11
# contain a '..', which seems to cause the rules to be incorrectly added
12
# and they are not removed when this script is re-run. Instead the new
13
# rules are simply appended. By using pwd we can get the full path
14
# without '..' and things work as expected.
15
OUTDIR="`( cd \"$OUTDIR\" && pwd) `"
16
NODE_RELEASE="$OUTDIR/Release/node"
17
NODE_DEBUG="$OUTDIR/Debug/node"
18
NODE_LINK="$ROOTDIR/node"
19
CCTEST_RELEASE="$OUTDIR/Release/cctest"
20
CCTEST_DEBUG="$OUTDIR/Debug/cctest"
21
OPENSSL_CLI_RELEASE="$OUTDIR/Release/openssl-cli"
22
OPENSSL_CLI_DEBUG="$OUTDIR/Debug/openssl-cli"
23
​
24
if [ -f $SFW ];
25
then
26
  # Duplicating these commands on purpose as the symbolic link node might be
27
  # linked to either out/Debug/node or out/Release/node depending on the
28
  # BUILDTYPE.
29
  $SFW --remove "$NODE_DEBUG"
30
  $SFW --remove "$NODE_DEBUG"
31
  $SFW --remove "$NODE_RELEASE"
32
  $SFW --remove "$NODE_RELEASE"
33
  $SFW --remove "$NODE_LINK"
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
