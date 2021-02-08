#!/bin/sh

# A word about this shell script:
#
# It must work everywhere, including on systems that lack
# a /bin/bash, map 'sh' to ksh, ksh97, bash, ash, or zsh,
# and potentially have either a posix shell or bourne
# shell living at /bin/sh.
#
# See this helpful document on writing portable shell scripts:
# http://www.gnu.org/s/hello/manual/autoconf/Portable-Shell.html
#
# The only shell it won't ever work on is cmd.exe.

if [ "x$0" = "xsh" ]; then
  # run as curl | sh
  # on some systems, you can just do cat>npm-install.sh
  # which is a bit cuter.  But on others, &1 is already closed,
  # so catting to another script file won't do anything.
  # Follow Location: headers, and fail on errors
  curl -q -f -L -s https://www.npmjs.org/install.sh > npm-install-$$.sh
  ret=$?
  if [ $ret -eq 0 ]; then
    (exit 0)
  else
    rm npm-install-$$.sh
    echo "failed to download script" >&2
    exit $ret
  fi
  sh npm-install-$$.sh
  ret=$?
  rm npm-install-$$.sh
  exit $ret
fi

debug=0
npm_config_loglevel="error"
if [ "x$npm_debug" = "x" ]; then
  (exit 0)
else
  echo "running in debug mode."
  echo "note that this requires bash or zsh."
  set -o xtrace
  set -o pipefail
  npm_config_loglevel="verbose"
  debug=1
fi
export npm_config_loglevel

# make sure that node exists
node=`which node 2>&1`
ret=$?
# if not found, try "nodejs" as it is the case on debian
if [ $ret -ne 0 ]; then
  node=`which nodejs 2>&1`
  ret=$?
fi
if [ $ret -eq 0 ] && [ -x "$node" ]; then
  if [ $debug -eq 1 ]; then
    echo "found 'node' at: $node"
    echo -n "version: "
    $node --version
    echo ""
  fi
  (exit 0)
else
  echo "npm cannot be installed without node.js." >&2
  echo "install node first, and then try again." >&2
  echo "" >&2
  exit $ret
fi

ret=0
tar="${TAR}"
if [ -z "$tar" ]; then
  tar="${npm_config_tar}"
fi
if [ -z "$tar" ]; then
  tar=`which tar 2>&1`
  ret=$?
fi

if [ $ret -eq 0 ] && [ -x "$tar" ]; then
  if [ $debug -eq 1 ]; then
    echo "found 'tar' at: $tar"
    echo -n "version: "
    $tar --version
    echo ""
  fi
  ret=$?
fi

if [ $ret -eq 0 ]; then
  (exit 0)
else
  echo "this script requires 'tar', please install it and try again."
  exit 1
fi

curl=`which curl 2>&1`
ret=$?
if [ $ret -eq 0 ]; then
  if [ $debug -eq 1 ]; then
    echo "found 'curl' at: $curl"
    echo -n "version: "
    $curl --version | head -n 1
    echo ""
  fi
  (exit 0)
else
  echo "this script requires 'curl', please install it and try again."
  exit 1
fi

# set the temp dir
TMP="${TMPDIR}"
if [ "x$TMP" = "x" ]; then
  TMP="/tmp"
fi
TMP="${TMP}/npm.$$"
rm -rf "$TMP" || true
mkdir "$TMP"
if [ $? -ne 0 ]; then
  echo "failed to mkdir $TMP" >&2
  exit 1
fi

BACK="$PWD"

t="${npm_install}"
if [ -z "$t" ]; then
  t="latest"
fi

# need to echo "" after, because Posix sed doesn't treat EOF
# as an implied end of line.
url=`(curl -qSsL https://registry.npmjs.org/npm/$t; echo "") \
     | sed -e 's/^.*tarball":"//' \
     | sed -e 's/".*$//'`

ret=$?
if [ "x$url" = "x" ]; then
  ret=125
  # try without the -e arg to sed.
  url=`(curl -qSsL https://registry.npmjs.org/npm/$t; echo "") \
       | sed 's/^.*tarball":"//' \
       | sed 's/".*$//'`
  ret=$?
  if [ "x$url" = "x" ]; then
    ret=125
  fi
fi
if [ $ret -ne 0 ]; then
  echo "failed to get tarball url for npm/$t" >&2
  exit $ret
fi


echo "fetching: $url" >&2

cd "$TMP" \
  && curl -qSsL -o npm.tgz "$url" \
  && $tar -xzf npm.tgz \
  && cd "$TMP"/package \
  && echo "removing existing npm" \
  && "$node" bin/npm-cli.js rm npm -gf --loglevel=silent \
  && echo "installing npm@$t" \
  && "$node" bin/npm-cli.js install -gf ../npm.tgz \
  && cd "$BACK" \
  && rm -rf "$TMP" \
  && echo "successfully installed npm@$t"

ret=$?
if [ $ret -ne 0 ]; then
  echo "failed!" >&2
fi
exit $ret
