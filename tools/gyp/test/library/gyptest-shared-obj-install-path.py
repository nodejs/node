#!/usr/bin/env python

# Copyright (c) 2010 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that .so files that are order only dependencies are specified by
their install location rather than by their alias.
"""

# Python 2.5 needs this for the with statement.
from __future__ import with_statement

import os
import TestGyp

test = TestGyp.TestGyp(formats=['make'])

test.run_gyp('shared_dependency.gyp',
             chdir='src')
test.relocate('src', 'relocate/src')

test.build('shared_dependency.gyp', test.ALL, chdir='relocate/src')

with open('relocate/src/Makefile') as makefile:
  make_contents = makefile.read()

# If we remove the code to generate lib1, Make should still be able
# to build lib2 since lib1.so already exists.
make_contents = make_contents.replace('include lib1.target.mk', '')
with open('relocate/src/Makefile', 'w') as makefile:
  makefile.write(make_contents)

test.build('shared_dependency.gyp', test.ALL, chdir='relocate/src')

test.pass_test()
