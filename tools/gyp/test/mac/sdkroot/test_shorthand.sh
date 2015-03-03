#!/bin/bash
# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

found=false
for sdk in 10.6 10.7 10.8 10.9 ; do
  if expected=$(xcodebuild -version -sdk macosx$sdk Path 2>/dev/null) ; then
    found=true
    break
  fi
done
if ! $found ; then
  echo >&2 "cannot find installed SDK"
  exit 1
fi

test $SDKROOT = $expected
