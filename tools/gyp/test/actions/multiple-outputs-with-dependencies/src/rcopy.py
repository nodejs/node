#!/usr/bin/env python
#
# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

"""A slightly odd 'cp' implementation for this test.

This 'cp' can have many targets, but only one source. 'cp src dest1 dest2'
will copy the file 'src' to both 'dest1' and 'dest2'."""

with open(sys.argv[1], 'r') as f:
  src = f.read()
for dest in sys.argv[2:]:
  with open(dest, 'w') as f:
    f.write(src)

