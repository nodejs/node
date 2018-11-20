#!/bin/sh

# find the name of the log file to process, it must not start with a dash.
log_file="v8.log"
for arg in "$@"
do
  if ! expr "X${arg}" : "^X-" > /dev/null; then
    log_file=${arg}
  fi
done

tools_path=`cd $(dirname "$0");pwd`
if [ ! "$D8_PATH" ]; then
  d8_public=`which d8`
  if [ -x "$d8_public" ]; then D8_PATH=$(dirname "$d8_public"); fi
fi
[ -n "$D8_PATH" ] || D8_PATH=$tools_path/..
d8_exec=$D8_PATH/d8

if [ ! -x "$d8_exec" ]; then
  D8_PATH=`pwd`/out/native
  d8_exec=$D8_PATH/d8
fi

if [ ! -x "$d8_exec" ]; then
  d8_exec=`grep -m 1 -o '".*/d8"' $log_file | sed 's/"//g'`
fi

if [ ! -x "$d8_exec" ]; then
  echo "d8 shell not found in $D8_PATH"
  echo "To build, execute 'make native' from the V8 directory"
  exit 1
fi

# nm spits out 'no symbols found' messages to stderr.
cat $log_file | $d8_exec --enable-os-system \
  $tools_path/splaytree.js $tools_path/codemap.js \
  $tools_path/csvparser.js $tools_path/consarray.js \
  $tools_path/profile.js $tools_path/profile_view.js \
  $tools_path/logreader.js $tools_path/arguments.js \
  $tools_path/tickprocessor.js $tools_path/SourceMap.js \
  $tools_path/tickprocessor-driver.js -- $@ 2>/dev/null
