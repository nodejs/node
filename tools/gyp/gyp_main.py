#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

# TODO(mark): sys.path manipulation is some temporary testing stuff.
try:
  import gyp
except ImportError, e:
  import os.path
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), 'pylib'))
  import gyp

if __name__ == '__main__':
  sys.exit(gyp.script_main())
