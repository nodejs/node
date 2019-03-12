#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure 'ninja_use_console' is supported in actions and rules.
"""

import TestGyp

test = TestGyp.TestGyp(formats=['ninja'])

test.run_gyp('use-console.gyp')

no_pool = open(test.built_file_path('obj/no_pool.ninja')).read()
if 'pool =' in no_pool:
  test.fail_test()

action_pool = open(test.built_file_path('obj/action_pool.ninja')).read()
if 'pool = console' not in action_pool:
  test.fail_test()

rule_pool = open(test.built_file_path('obj/rule_pool.ninja')).read()
if 'pool = console' not in rule_pool:
  test.fail_test()

test.pass_test()
