#!/usr/bin/env python
# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import sys

data = open(sys.argv[3], 'r').read()
fh = open(sys.argv[4], 'w')
fh.write(data.replace(sys.argv[1], sys.argv[2]))
fh.close()
