#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Runs small tests.
"""

import imp
import os
import sys
import unittest


# Add pylib to the import path (so tests can import their dependencies).
# This is consistent with the path.append done in the top file "gyp".
# This need to happen before we init TestGyp, which changes the cwd.
gyp_src_root = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
sys.path.insert(0, gyp_src_root)


files_to_test = [
  'common_test.py',
  'easy_xml_test.py',
  'generator_msvs_test.py',
  'NinjaWriter_test.py',
  'generator_xcode_test.py',
  'input_test.py',
  'MSVSSettings_test.py',
]
# Add new test suites here.

# Collect all the suites from the above files.
suites = []
for filename in files_to_test:
  # Carve the module name out of the path.
  name = os.path.splitext(filename)[0]
  # Find the complete module path.
  full_filename = os.path.join(gyp_src_root, 'gyp', 'unit_tests', filename)
  # Load the module.
  module = imp.load_source(name, full_filename)
  # Add it to the list of test suites.
  suites.append(unittest.defaultTestLoader.loadTestsFromModule(module))
# Create combined suite.
all_tests = unittest.TestSuite(suites)

# Run all the tests.
result = unittest.TextTestRunner(verbosity=2).run(all_tests)
if result.failures or result.errors:
  sys.exit(1)

