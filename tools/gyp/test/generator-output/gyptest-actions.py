#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies --generator-output= behavior when using actions.
"""

import TestGyp

# Android doesn't support --generator-output.
test = TestGyp.TestGyp(formats=['!android'])

# All the generated files should go under 'gypfiles'. The source directory
# ('actions') should be untouched.
test.writable(test.workpath('actions'), False)
test.run_gyp('actions.gyp',
             '--generator-output=' + test.workpath('gypfiles'),
             chdir='actions')

test.writable(test.workpath('actions'), True)

test.relocate('actions', 'relocate/actions')
test.relocate('gypfiles', 'relocate/gypfiles')

test.writable(test.workpath('relocate/actions'), False)

# Some of the action outputs use "pure" relative paths (i.e. without prefixes
# like <(INTERMEDIATE_DIR) or <(PROGRAM_DIR)). Even though we are building under
# 'gypfiles', such outputs will still be created relative to the original .gyp
# sources. Projects probably wouldn't normally do this, since it kind of defeats
# the purpose of '--generator-output', but it is supported behaviour.
test.writable(test.workpath('relocate/actions/build'), True)
test.writable(test.workpath('relocate/actions/subdir1/build'), True)
test.writable(test.workpath('relocate/actions/subdir1/actions-out'), True)
test.writable(test.workpath('relocate/actions/subdir2/build'), True)
test.writable(test.workpath('relocate/actions/subdir2/actions-out'), True)

test.build('actions.gyp', test.ALL, chdir='relocate/gypfiles')

expect = """\
Hello from program.c
Hello from make-prog1.py
Hello from make-prog2.py
"""

if test.format == 'xcode':
  chdir = 'relocate/actions/subdir1'
else:
  chdir = 'relocate/gypfiles'
test.run_built_executable('program', chdir=chdir, stdout=expect)

test.must_match('relocate/actions/subdir2/actions-out/file.out',
                "Hello from make-file.py\n")

test.pass_test()
