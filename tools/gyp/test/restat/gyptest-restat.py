#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verify that dependent rules are executed iff a dependency action modifies its
outputs.
"""

import TestGyp
import os

test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

test.run_gyp('restat.gyp', chdir='src')

chdir = 'relocate/src'
test.relocate('src', chdir)

# Building 'dependent' the first time generates 'side_effect', but building it
# the second time doesn't, because 'create_intermediate' doesn't update its
# output.
test.build('restat.gyp', 'dependent', chdir=chdir)
test.built_file_must_exist('side_effect', chdir=chdir)
os.remove(test.built_file_path('side_effect', chdir=chdir))
test.build('restat.gyp', 'dependent', chdir=chdir)
test.built_file_must_not_exist('side_effect', chdir=chdir)

test.pass_test()
