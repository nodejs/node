#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure LTCG setting is extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'lib-flags'
  test.run_gyp('ltcg.gyp', chdir=CHDIR)
  test.build('ltcg.gyp', test.ALL, chdir=CHDIR)
  test.must_not_contain_any_line(test.stdout(), ['restarting link with /LTCG'])
  test.pass_test()
