#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verify the settings that cause a set of programs to be created in
a specific build directory, and that no intermediate built files
get created outside of that build directory hierarchy even when
referred to with deeply-nested ../../.. paths.
"""

import TestGyp

# TODO(mmoss): Make only supports (theoretically) a single, global build
# directory (through GYP_GENERATOR_FLAGS 'output_dir'), rather than
# gyp-file-specific settings (e.g. the stuff in builddir.gypi) that the other
# generators support, so this doesn't work yet for make.
# TODO(mmoss) Make also has the issue that the top-level Makefile is written to
# the "--depth" location, which is one level above 'src', but then this test
# moves 'src' somewhere else, leaving the Makefile behind, so make can't find
# its sources. I'm not sure if make is wrong for writing outside the current
# directory, or if the test is wrong for assuming everything generated is under
# the current directory.
# Android does not support setting the build directory.
test = TestGyp.TestGyp(formats=['!make', '!ninja', '!android'])

test.run_gyp('prog1.gyp', '--depth=..', chdir='src')
if test.format == 'msvs':
  if test.uses_msbuild:
    test.must_contain('src/prog1.vcxproj',
      '<OutDir>..\\builddir\\Default\\</OutDir>')
  else:
    test.must_contain('src/prog1.vcproj',
      'OutputDirectory="..\\builddir\\Default\\"')

test.relocate('src', 'relocate/src')

test.subdir('relocate/builddir')

# Make sure that all the built ../../etc. files only get put under builddir,
# by making all of relocate read-only and then making only builddir writable.
test.writable('relocate', False)
test.writable('relocate/builddir', True)

# Suppress the test infrastructure's setting SYMROOT on the command line.
test.build('prog1.gyp', test.ALL, SYMROOT=None, chdir='relocate/src')

expect1 = """\
Hello from prog1.c
Hello from func1.c
"""

expect2 = """\
Hello from subdir2/prog2.c
Hello from func2.c
"""

expect3 = """\
Hello from subdir2/subdir3/prog3.c
Hello from func3.c
"""

expect4 = """\
Hello from subdir2/subdir3/subdir4/prog4.c
Hello from func4.c
"""

expect5 = """\
Hello from subdir2/subdir3/subdir4/subdir5/prog5.c
Hello from func5.c
"""

def run_builddir(prog, expect):
  dir = 'relocate/builddir/Default/'
  test.run(program=test.workpath(dir + prog), stdout=expect)

run_builddir('prog1', expect1)
run_builddir('prog2', expect2)
run_builddir('prog3', expect3)
run_builddir('prog4', expect4)
run_builddir('prog5', expect5)

test.pass_test()
