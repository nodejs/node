#!/usr/bin/env python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies simplest-possible build of a "Hello, world!" program
using the default build target.
"""

import TestGyp

test = TestGyp.TestGyp(workdir='workarea_default', formats=['msvs'])

# Run from down in foo.
test.run_gyp('a.gyp', chdir='foo/a')
sln = test.workpath('foo/a/a.sln')
sln_data = open(sln, 'rb').read().decode('utf-8', 'ignore')
vcproj = sln_data.count('b.vcproj')
vcxproj = sln_data.count('b.vcxproj')
if (vcproj, vcxproj) not in [(1, 0), (0, 1)]:
  test.fail_test()

test.pass_test()
