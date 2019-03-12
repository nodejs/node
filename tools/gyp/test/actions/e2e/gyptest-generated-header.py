#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that dependencies on generated headers work, even if the header has
a mixed-case file name.
"""

import TestGyp

test = TestGyp.TestGyp()

CHDIR = 'generated-header'

test.run_gyp('test.gyp', chdir=CHDIR)
test.build('test.gyp', 'program', chdir=CHDIR)
test.up_to_date('test.gyp', 'program', chdir=CHDIR)

expect = 'foobar output\n'
test.run_built_executable('program', chdir=CHDIR, stdout=expect)

# Change what's written to the generated header, regyp and rebuild, and check
# that the change makes it to the executable and that the build is clean.
test.sleep()
content = test.read('generated-header/test.gyp').replace('foobar', 'barbaz')
test.write('generated-header/test.gyp', content)

test.run_gyp('test.gyp', chdir=CHDIR)
test.build('test.gyp', 'program', chdir=CHDIR)
test.up_to_date('test.gyp', 'program', chdir=CHDIR)

expect = 'barbaz output\n'
test.run_built_executable('program', chdir=CHDIR, stdout=expect)

test.pass_test()
