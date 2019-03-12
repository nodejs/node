#!/usr/bin/python
# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

fh = open(sys.argv[1], 'w')

filenames = sys.argv[2:]

for filename in filenames:
  subfile = open(filename)
  data = subfile.read()
  subfile.close()
  fh.write(data)

fh.close()
