#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies file copies where two copies sections in the same target have the
same destination directory.
"""

import TestGyp

# The Android build system doesn't allow output to go to arbitrary places.
test = TestGyp.TestGyp(formats=['!android'])
test.run_gyp('copies-samedir.gyp', chdir='src')

test.relocate('src', 'relocate/src')
test.build('copies-samedir.gyp', 'copies_samedir', chdir='relocate/src')

test.built_file_must_match('copies-out-samedir/file1',
                           'file1 contents\n',
                           chdir='relocate/src')

test.built_file_must_match('copies-out-samedir/file2',
                           'file2 contents\n',
                           chdir='relocate/src')

test.pass_test()
