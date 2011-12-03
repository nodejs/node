#!/usr/bin/env python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Runs small tests.
"""

import imp
import os
import sys
import unittest

import TestGyp


test = TestGyp.TestGyp()

# Add pylib to the import path (so tests can import their dependencies).
# This is consistant with the path.append done in the top file "gyp".
sys.path.append(os.path.join(test._cwd, 'pylib'))

# Add new test suites here.
files_to_test = [
    'pylib/gyp/MSVSSettings_test.py',
    'pylib/gyp/easy_xml_test.py',
    'pylib/gyp/generator/msvs_test.py',
]

# Collect all the suites from the above files.
suites = []
for filename in files_to_test:
  # Carve the module name out of the path.
  name = os.path.splitext(os.path.split(filename)[1])[0]
  # Find the complete module path.
  full_filename = os.path.join(test._cwd, filename)
  # Load the module.
  module = imp.load_source(name, full_filename)
  # Add it to the list of test suites.
  suites.append(unittest.defaultTestLoader.loadTestsFromModule(module))
# Create combined suite.
all_tests = unittest.TestSuite(suites)

# Run all the tests.
result = unittest.TextTestRunner(verbosity=2).run(all_tests)
if result.failures or result.errors:
  test.fail_test()

test.pass_test()
