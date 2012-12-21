#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that a default gyp define can be overridden.
"""

import os
import TestGyp

test = TestGyp.TestGyp()

# Command-line define
test.run_gyp('defines.gyp', '-D', 'OS=fakeos')
test.build('defines.gyp')
test.built_file_must_exist('fakeosprogram', type=test.EXECUTABLE)
# Clean up the exe so subsequent tests don't find an old exe.
os.remove(test.built_file_path('fakeosprogram', type=test.EXECUTABLE))

# Without "OS" override, fokeosprogram shouldn't be built.
test.run_gyp('defines.gyp')
test.build('defines.gyp')
test.built_file_must_not_exist('fakeosprogram', type=test.EXECUTABLE)

# Environment define
os.environ['GYP_DEFINES'] = 'OS=fakeos'
test.run_gyp('defines.gyp')
test.build('defines.gyp')
test.built_file_must_exist('fakeosprogram', type=test.EXECUTABLE)

test.pass_test()
