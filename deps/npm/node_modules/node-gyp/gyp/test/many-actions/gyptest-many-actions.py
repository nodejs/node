#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure lots of actions in the same target don't cause exceeding command
line length.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('many-actions.gyp')
test.build('many-actions.gyp', test.ALL)
for i in range(200):
  test.built_file_must_exist('generated_%d.h' % i)
test.pass_test()
