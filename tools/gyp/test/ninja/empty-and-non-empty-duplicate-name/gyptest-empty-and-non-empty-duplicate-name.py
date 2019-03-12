#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies a phony target isn't output if a target exists with the same name that
was output.
"""

import TestGyp

test = TestGyp.TestGyp(formats=['ninja'])

# Reset xcode_ninja_target_pattern to its default for this test.
test.run_gyp('test.gyp', '-G', 'xcode_ninja_target_pattern=^$')

# Check for both \r and \n to cover both windows and linux.
test.must_not_contain('out/Default/build.ninja', 'build empty_target: phony\r')
test.must_not_contain('out/Default/build.ninja', 'build empty_target: phony\n')

test.pass_test()
