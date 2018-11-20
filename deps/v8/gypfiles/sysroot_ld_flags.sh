#!/bin/sh
# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is for backwards-compatibility after:
# https://codereview.chromium.org/2900193003

for entry in $@; do
  echo -L$entry
  echo -Wl,-rpath-link=$entry
done | xargs echo
