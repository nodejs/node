#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure we don't cause unnecessary builds due to import libs appearing
to be out of date.
"""

import TestGyp

import sys
import time

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'importlib'
  test.run_gyp('importlib.gyp', chdir=CHDIR)
  test.build('importlib.gyp', test.ALL, chdir=CHDIR)

  # Delay briefly so that there's time for this touch not to have the
  # timestamp as the previous run.
  test.sleep()

  # Touch the .cc file; the .dll will rebuild, but the import libs timestamp
  # won't be updated.
  test.touch('importlib/has-exports.cc')
  test.build('importlib.gyp', 'test_importlib', chdir=CHDIR)

  # This is the important part. The .dll above will relink and have an updated
  # timestamp, however the import .libs timestamp won't be updated. So, we
  # have to handle restating inputs in ninja so the final binary doesn't
  # continually relink (due to thinking the .lib isn't up to date).
  test.up_to_date('importlib.gyp', test.ALL, chdir=CHDIR)

  test.pass_test()
