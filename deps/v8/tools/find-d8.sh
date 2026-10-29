#!/bin/bash
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

log_file="$1"
tools_path=$(cd "$(dirname "$0")" && pwd)

if [ ! "$D8_PATH" ]; then
  d8_public=$(which d8)
  if [ -x "$d8_public" ]; then
    D8_PATH=$(dirname "$d8_public")
  fi
fi
if [ -z "${D8_PATH##*/d8}" ]; then
  d8_exec=$D8_PATH
else
  d8_exec=$D8_PATH/d8
fi

if [ ! -x "$d8_exec" ]; then
  for platform in x64 arm64 ia32; do
    for release in release optdebug debug; do
      d8_candidate="${tools_path}/../out/${platform}.${release}/d8"
      if [ -x "$d8_candidate" ]; then
        d8_exec="$d8_candidate"
        break 2
      fi
    done
  done
fi

if [ ! -x "$d8_exec" ]; then
  jsvu_v8="$HOME/.jsvu/bin/v8"
  if [ -x "$jsvu_v8" ]; then
    d8_exec="$jsvu_v8"
  fi
fi

if [ ! -x "$d8_exec" ] && [ -f "$log_file" ]; then
  d8_exec=$(grep -m 1 -o '".*/d8"' "$log_file" | sed 's/"//g')
fi

if [ -x "$d8_exec" ]; then
  echo "$d8_exec"
  exit 0
else
  echo "d8 shell not found" >&2
  echo "Please provide path to d8 as env var in D8_PATH" >&2
  exit 1
fi
