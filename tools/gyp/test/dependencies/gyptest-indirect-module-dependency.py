#!/usr/bin/env python

# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure that we cause downstream modules to get built when we depend on the
parent targets.
"""

import TestGyp

test = TestGyp.TestGyp()

CHDIR = 'module-dep'
test.run_gyp('indirect-module-dependency.gyp', chdir=CHDIR)
test.build('indirect-module-dependency.gyp', 'an_exe', chdir=CHDIR)
test.built_file_must_exist(
    test.built_file_basename('a_module', test.LOADABLE_MODULE), chdir=CHDIR)

test.pass_test()
