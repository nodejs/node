#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Verifies that depfile fields are output in ninja rules."""

import TestGyp

test = TestGyp.TestGyp()

if test.format == 'ninja':
  test.run_gyp('depfile.gyp')
  contents = open(test.built_file_path('obj/depfile_target.ninja')).read()

  expected = 'depfile = depfile.d'
  if expected not in contents:
    test.fail_test()
  test.pass_test()
