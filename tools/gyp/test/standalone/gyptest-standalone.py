#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that a project hierarchy created with the --generator-output=
option can be built even when it's relocated to a different path.
"""

from __future__ import print_function

import TestGyp
import os

test = TestGyp.TestGyp()

test.run_gyp('standalone.gyp', '-Gstandalone')

# Look at all the files in the tree to make sure none
# of them reference the gyp file.
me = os.path.basename(__file__)
for root, dirs, files in os.walk("."):
  for file in files:
    # ignore me
    if me in file:
      continue
    file = os.path.join(root, file)
    contents = test.read(file)
    if 'standalone.gyp' in contents:
      print('gyp file referenced in generated output: %s' % file)
      test.fail_test()


test.pass_test()
