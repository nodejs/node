#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verify that running gyp in a different directory does not cause actions and
rules to rerun.
"""

import os
import sys
import TestGyp

test = TestGyp.TestGyp(formats=['ninja'])
# The xcode-ninja generator handles gypfiles which are not at the
# project root incorrectly.
# cf. https://code.google.com/p/gyp/issues/detail?id=460
if test.format == 'xcode-ninja':
  test.skip_test()

test.run_gyp('subdir/action-rule-hash.gyp')
test.build('subdir/action-rule-hash.gyp', test.ALL)
test.up_to_date('subdir/action-rule-hash.gyp')

# Verify that everything is still up-to-date when we re-invoke gyp from a
# different directory.
test.run_gyp('action-rule-hash.gyp', '--depth=../', chdir='subdir')
test.up_to_date('subdir/action-rule-hash.gyp')

test.pass_test()
