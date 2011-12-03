#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that a rule that generates multiple outputs rebuilds
correctly when the inputs change.
"""

import TestGyp

test = TestGyp.TestGyp(workdir='workarea_default')

test.run_gyp('same_target.gyp', chdir='src')

test.relocate('src', 'relocate/src')


test.build('same_target.gyp', chdir='relocate/src')

expect = """\
Hello from main.c
Hello from prog1.in!
Hello from prog2.in!
"""

test.run_built_executable('program', chdir='relocate/src', stdout=expect)

test.up_to_date('same_target.gyp', 'program', chdir='relocate/src')


test.sleep()
contents = test.read(['relocate', 'src', 'prog1.in'])
contents = contents.replace('!', ' AGAIN!')
test.write(['relocate', 'src', 'prog1.in'], contents)

test.build('same_target.gyp', chdir='relocate/src')

expect = """\
Hello from main.c
Hello from prog1.in AGAIN!
Hello from prog2.in!
"""

test.run_built_executable('program', chdir='relocate/src', stdout=expect)

test.up_to_date('same_target.gyp', 'program', chdir='relocate/src')


test.sleep()
contents = test.read(['relocate', 'src', 'prog2.in'])
contents = contents.replace('!', ' AGAIN!')
test.write(['relocate', 'src', 'prog2.in'], contents)

test.build('same_target.gyp', chdir='relocate/src')

expect = """\
Hello from main.c
Hello from prog1.in AGAIN!
Hello from prog2.in AGAIN!
"""

test.run_built_executable('program', chdir='relocate/src', stdout=expect)

test.up_to_date('same_target.gyp', 'program', chdir='relocate/src')


test.pass_test()
