#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verify handling of build variants.

TODO:  Right now, only the SCons generator supports this, so the
test case is SCons-specific.  In particular, it relise on SCons'
ability to rebuild in response to changes on the command line.  It
may be simpler to just drop this feature if the other generators
can't be made to behave the same way.
"""

import TestGyp

test = TestGyp.TestGyp(formats=['scons'])

test.run_gyp('variants.gyp', chdir='src')

test.relocate('src', 'relocate/src')

test.build('variants.gyp', chdir='relocate/src')

test.run_built_executable('variants',
                          chdir='relocate/src',
                          stdout="Hello, world!\n")

test.sleep()
test.build('variants.gyp', 'VARIANT1=1', chdir='relocate/src')

test.run_built_executable('variants',
                          chdir='relocate/src',
                          stdout="Hello from VARIANT1\n")

test.sleep()
test.build('variants.gyp', 'VARIANT2=1', chdir='relocate/src')

test.run_built_executable('variants',
                          chdir='relocate/src',
                          stdout="Hello from VARIANT2\n")

test.pass_test()
