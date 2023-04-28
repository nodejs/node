#!/usr/bin/env bash
(set -o igncr) 2>/dev/null && set -o igncr; # cygwin encoding fix

basedir=`dirname "$0"`

case `uname` in
  *CYGWIN*) basedir=`cygpath -w "$basedir"`;;
esac

NODE_EXE="$basedir/node.exe"
if ! [ -x "$NODE_EXE" ]; then
  NODE_EXE="$basedir/node"
fi
if ! [ -x "$NODE_EXE" ]; then
  NODE_EXE=node
fi

# this path is passed to node.exe, so it needs to match whatever
# kind of paths Node.js thinks it's using, typically win32 paths.
CLI_BASEDIR="$("$NODE_EXE" -p 'require("path").dirname(process.execPath)')"
NPM_CLI_JS="$CLI_BASEDIR/node_modules/npm/bin/npm-cli.js"

NPM_PREFIX=`"$NODE_EXE" "$NPM_CLI_JS" prefix -g`
if [ $? -ne 0 ]; then
  # if this didn't work, then everything else below will fail
  echo "Could not determine Node.js install directory" >&2
  exit 1
fi
NPM_PREFIX_NPM_CLI_JS="$NPM_PREFIX/node_modules/npm/bin/npm-cli.js"

# a path that will fail -f test on any posix bash
NPM_WSL_PATH="/.."

# WSL can run Windows binaries, so we have to give it the win32 path
# however, WSL bash tests against posix paths, so we need to construct that
# to know if npm is installed globally.
if [ `uname` = 'Linux' ] && type wslpath &>/dev/null ; then
  NPM_WSL_PATH=`wslpath "$NPM_PREFIX_NPM_CLI_JS"`
fi
if [ -f "$NPM_PREFIX_NPM_CLI_JS" ] || [ -f "$NPM_WSL_PATH" ]; then
  NPM_CLI_JS="$NPM_PREFIX_NPM_CLI_JS"
fi

"$NODE_EXE" "$NPM_CLI_JS" "$@"
