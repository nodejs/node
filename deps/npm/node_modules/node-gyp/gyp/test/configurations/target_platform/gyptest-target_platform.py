#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Tests the msvs specific msvs_target_platform option.
"""

import TestGyp
import TestCommon


def RunX64(exe, stdout):
  try:
    test.run_built_executable(exe, stdout=stdout)
  except WindowsError, e:
    # Assume the exe is 64-bit if it can't load on 32-bit systems.
    # Both versions of the error are required because different versions
    # of python seem to return different errors for invalid exe type.
    if e.errno != 193 and '[Error 193]' not in str(e):
      raise


test = TestGyp.TestGyp(formats=['msvs'])

test.run_gyp('configurations.gyp')

test.set_configuration('Debug|x64')
test.build('configurations.gyp', rebuild=True)
RunX64('front_left', stdout=('left\n'))
RunX64('front_right', stdout=('right\n'))

test.set_configuration('Debug|Win32')
test.build('configurations.gyp', rebuild=True)
RunX64('front_left', stdout=('left\n'))
test.run_built_executable('front_right', stdout=('right\n'))

test.pass_test()
