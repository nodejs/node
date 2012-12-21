#!/usr/bin/python
# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import sys

fh = open(sys.argv[-1], 'wb')
for filename in sys.argv[1:-1]:
  fh.write(open(filename).read())
fh.close()
