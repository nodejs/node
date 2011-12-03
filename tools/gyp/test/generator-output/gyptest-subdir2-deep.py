#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies building a target from a .gyp file a few subdirectories
deep when the --generator-output= option is used to put the build
configuration files in a separate directory tree.
"""

import TestGyp

test = TestGyp.TestGyp(formats=['!ninja'])

test.writable(test.workpath('src'), False)

test.writable(test.workpath('src/subdir2/deeper/build'), True)

test.run_gyp('deeper.gyp',
             '-Dset_symroot=1',
             '--generator-output=' + test.workpath('gypfiles'),
             chdir='src/subdir2/deeper')

test.build('deeper.gyp', test.ALL, chdir='gypfiles')

chdir = 'gypfiles'

if test.format == 'xcode':
  chdir = 'src/subdir2/deeper'
test.run_built_executable('deeper',
                          chdir=chdir,
                          stdout="Hello from deeper.c\n")

test.pass_test()
