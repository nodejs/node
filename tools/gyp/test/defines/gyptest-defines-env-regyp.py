#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies build of an executable with C++ define specified by a gyp define, and
the use of the environment during regeneration when the gyp file changes.
"""

import os
import TestGyp

# Regenerating build files when a gyp file changes is currently only supported
# by the make generator.
test = TestGyp.TestGyp(formats=['make'])

try:
  os.environ['GYP_DEFINES'] = 'value=50'
  test.run_gyp('defines.gyp')
finally:
  # We clear the environ after calling gyp.  When the auto-regeneration happens,
  # the same define should be reused anyway.  Reset to empty string first in
  # case the platform doesn't support unsetenv.
  os.environ['GYP_DEFINES'] = ''
  del os.environ['GYP_DEFINES']

test.build('defines.gyp')

expect = """\
FOO is defined
VALUE is 1
2*PAREN_VALUE is 12
HASH_VALUE is a#1
"""
test.run_built_executable('defines', stdout=expect)

# Sleep so that the changed gyp file will have a newer timestamp than the
# previously generated build files.
test.sleep()
test.write('defines.gyp', test.read('defines-env.gyp'))

test.build('defines.gyp', test.ALL)

expect = """\
VALUE is 50
"""
test.run_built_executable('defines', stdout=expect)

test.pass_test()
