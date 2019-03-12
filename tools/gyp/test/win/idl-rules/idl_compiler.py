#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# mock, just outputs empty .h/.cpp files

import os
import sys

if len(sys.argv) == 2:
  basename, ext = os.path.splitext(sys.argv[1])
  with open('%s.h' % basename, 'w') as f:
    f.write('// %s.h\n' % basename)
  with open('%s.cpp' % basename, 'w') as f:
    f.write('// %s.cpp\n' % basename)
