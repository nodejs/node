#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies simple build of a "Hello, world!" program with shared libraries,
including verifying that libraries are rebuilt correctly when functions
move between libraries.
"""

import TestGyp

test = TestGyp.TestGyp()

if test.format == 'android':
  # This test currently fails on android. Investigate why, fix the issues
  # responsible, and reenable this test on android. See bug:
  # https://code.google.com/p/gyp/issues/detail?id=436
  test.skip_test(message='Test fails on android. Fix and reenable.\n')

test.run_gyp('library.gyp',
             '-Dlibrary=shared_library',
             '-Dmoveable_function=lib1',
             chdir='src')

test.relocate('src', 'relocate/src')

test.build('library.gyp', test.ALL, chdir='relocate/src')

expect = """\
Hello from program.c
Hello from lib1.c
Hello from lib2.c
Hello from lib1_moveable.c
"""
test.run_built_executable('program', chdir='relocate/src', stdout=expect)


test.run_gyp('library.gyp',
             '-Dlibrary=shared_library',
             '-Dmoveable_function=lib2',
             chdir='relocate/src')

# Update program.c to force a rebuild.
test.sleep()
contents = test.read('relocate/src/program.c')
contents = contents.replace('Hello', 'Hello again')
test.write('relocate/src/program.c', contents)

test.build('library.gyp', test.ALL, chdir='relocate/src')

expect = """\
Hello again from program.c
Hello from lib1.c
Hello from lib2.c
Hello from lib2_moveable.c
"""
test.run_built_executable('program', chdir='relocate/src', stdout=expect)


test.run_gyp('library.gyp',
             '-Dlibrary=shared_library',
             '-Dmoveable_function=lib1',
             chdir='relocate/src')

# Update program.c to force a rebuild.
test.sleep()
contents = test.read('relocate/src/program.c')
contents = contents.replace('again', 'again again')
test.write('relocate/src/program.c', contents)

# TODO(sgk):  we have to force a rebuild of lib2 so that it weeds out
# the "moved" module.  This should be done in gyp by adding a dependency
# on the generated .vcproj file itself.
test.touch('relocate/src/lib2.c')

test.build('library.gyp', test.ALL, chdir='relocate/src')

expect = """\
Hello again again from program.c
Hello from lib1.c
Hello from lib2.c
Hello from lib1_moveable.c
"""
test.run_built_executable('program', chdir='relocate/src', stdout=expect)


test.pass_test()
