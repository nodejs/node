#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure msvs_shard works correctly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'shard'
  test.run_gyp('shard.gyp', chdir=CHDIR)
  test.build('shard.gyp', test.ALL, chdir=CHDIR)

  test.built_file_must_exist('shard_0.lib', chdir=CHDIR)
  test.built_file_must_exist('shard_1.lib', chdir=CHDIR)
  test.built_file_must_exist('shard_2.lib', chdir=CHDIR)
  test.built_file_must_exist('shard_3.lib', chdir=CHDIR)

  test.run_gyp('shard_ref.gyp', chdir=CHDIR)
  test.build('shard_ref.gyp', test.ALL, chdir=CHDIR)

  test.pass_test()
