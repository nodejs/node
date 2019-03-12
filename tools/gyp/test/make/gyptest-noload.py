#!/usr/bin/env python

# Copyright (c) 2010 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Tests the use of the NO_LOAD flag which makes loading sub .mk files
optional.
"""

# Python 2.5 needs this for the with statement.
from __future__ import with_statement

import os
import TestGyp

test = TestGyp.TestGyp(formats=['make'])

test.run_gyp('all.gyp', chdir='noload')

test.relocate('noload', 'relocate/noload')

test.build('build/all.gyp', test.ALL, chdir='relocate/noload')
test.run_built_executable('exe', chdir='relocate/noload',
                          stdout='Hello from shared.c.\n')

# Just sanity test that NO_LOAD=lib doesn't break anything.
test.build('build/all.gyp', test.ALL, chdir='relocate/noload',
           arguments=['NO_LOAD=lib'])
test.run_built_executable('exe', chdir='relocate/noload',
                          stdout='Hello from shared.c.\n')
test.build('build/all.gyp', test.ALL, chdir='relocate/noload',
           arguments=['NO_LOAD=z'])
test.run_built_executable('exe', chdir='relocate/noload',
                          stdout='Hello from shared.c.\n')

# Make sure we can rebuild without reloading the sub .mk file.
with open('relocate/noload/main.c', 'a') as src_file:
  src_file.write("\n")
test.build('build/all.gyp', test.ALL, chdir='relocate/noload',
           arguments=['NO_LOAD=lib'])
test.run_built_executable('exe', chdir='relocate/noload',
                          stdout='Hello from shared.c.\n')

# Change shared.c, but verify that it doesn't get rebuild if we don't load it.
with open('relocate/noload/lib/shared.c', 'w') as shared_file:
  shared_file.write(
      '#include "shared.h"\n'
      'const char kSharedStr[] = "modified";\n'
  )
test.build('build/all.gyp', test.ALL, chdir='relocate/noload',
           arguments=['NO_LOAD=lib'])
test.run_built_executable('exe', chdir='relocate/noload',
                          stdout='Hello from shared.c.\n')

test.pass_test()
