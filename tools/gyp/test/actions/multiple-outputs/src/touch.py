#!/usr/bin/env python
#
# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

"""Cross-platform touch."""

for fname in sys.argv[1:]:
  if os.path.exists(fname):
    os.utime(fname, None)
  else:
    open(fname, 'w').close()
