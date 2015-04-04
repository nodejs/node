#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verify that link_settings in a shared_library are not propagated to targets
that depend on the shared_library, but are used in the shared_library itself.
"""

import TestGyp
import sys

CHDIR='sharedlib-linksettings'

test = TestGyp.TestGyp()
test.run_gyp('test.gyp', chdir=CHDIR)
test.build('test.gyp', test.ALL, chdir=CHDIR)
test.run_built_executable('program', stdout="1\n2\n", chdir=CHDIR)
test.pass_test()
