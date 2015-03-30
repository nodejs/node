#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies file copies where the destination is one level above an expansion that
yields a make variable.
"""

import TestGyp

# The Android build system doesn't allow output to go to arbitrary places.
test = TestGyp.TestGyp(formats=['!android'])
test.run_gyp('copies-updir.gyp', chdir='src')

test.relocate('src', 'relocate/src')
test.build('copies-updir.gyp', 'copies_up', chdir='relocate/src')

test.built_file_must_match('../copies-out-updir/file1',
                           'file1 contents\n',
                           chdir='relocate/src')

test.pass_test()
