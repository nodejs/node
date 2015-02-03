#!/usr/bin/env python

# Copyright 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Test variable expansion of '<!()' syntax commands where they are evaluated
more than once from different directories.
"""

import TestGyp

test = TestGyp.TestGyp()

# This tests GYP's cache of commands, ensuring that the directory a command is
# run from is part of its cache key. Parallelism may lead to multiple cache
# lookups failing, resulting in the command being run multiple times by
# chance, not by GYP's logic. Turn off parallelism to ensure that the logic is
# being tested.
test.run_gyp('repeated_multidir/main.gyp', '--no-parallel')

test.pass_test()
