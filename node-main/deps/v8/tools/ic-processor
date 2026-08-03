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
  if [ -x "$d8_public" ]; then
    D8_PATH=$(dirname "$d8_public");
  fi
fi
if [ -z ${D8_PATH##*/d8} ]; then
  d8_exec=$D8_PATH
else
  d8_exec=$D8_PATH/d8
fi

if [ ! -x "$d8_exec" ]; then
  for platform in x64 arm64 ia32; do
    for release in release optdebug debug; do
      if [ -x "$d8_exec" ]; then
        continue
      fi
      d8_exec="${tools_path}/../out/${platform}.${release}/d8";
    done
  done
fi

if [ ! -x "$d8_exec" ]; then
  d8_exec=`grep -m 1 -o '".*/d8"' $log_file | sed 's/"//g'`;
fi

if [ ! -x "$d8_exec" ]; then
  echo "d8 shell not found in $D8_PATH"
  echo "To build, execute 'make native' from the V8 directory"
  exit 1
fi

# nm spits out 'no symbols found' messages to stderr.
cat $log_file | $d8_exec \
  --module $tools_path/ic-processor-driver.mjs -- $@ 2>/dev/null
