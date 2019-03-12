#!/usr/bin/env python

# Copyright 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Tests that all_dependent_settings are processed in topological order.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('all_dependent_settings_order.gyp', chdir='adso')
test.build('all_dependent_settings_order.gyp', chdir='adso')
test.built_file_must_match('out.txt', 'd.cc a.cc b.cc c.cc',
                           chdir='adso')
test.pass_test()
