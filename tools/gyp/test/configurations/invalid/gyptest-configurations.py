#!/usr/bin/env python

# Copyright (c) 2010 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies build of an executable in three different configurations.
"""

import TestGyp

# Keys that do not belong inside a configuration dictionary.
invalid_configuration_keys = [
  'actions',
  'all_dependent_settings',
  'configurations',
  'dependencies',
  'direct_dependent_settings',
  'libraries',
  'link_settings',
  'sources',
  'standalone_static_library',
  'target_name',
  'type',
]

test = TestGyp.TestGyp()

for test_key in invalid_configuration_keys:
  test.run_gyp('%s.gyp' % test_key, status=1, stderr=None)
  expect = ['%s not allowed in the Debug configuration, found in target '
            '%s.gyp:configurations#target' % (test_key, test_key)]
  test.must_contain_all_lines(test.stderr(), expect)

test.pass_test()
