#!/usr/bin/python
# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

src = open(sys.argv[1])
dst = open(sys.argv[2], 'w')
for ch in src.read():
  dst.write('%d,\n' % ord(ch))
src.close()
dst.close()
