#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure optimization settings are extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['ninja'])

  CHDIR = 'compiler-flags'
  test.run_gyp('optimizations.gyp', chdir=CHDIR)

  # It's hard to map flags to output contents in a non-fragile way (especially
  # handling both 2008/2010), so just verify the correct ninja command line
  # contents.

  ninja_file = test.built_file_path('obj/test_opt_off.ninja', chdir=CHDIR)
  test.must_contain(ninja_file, 'cflags = /Od')

  ninja_file = test.built_file_path('obj/test_opt_lev_size.ninja', chdir=CHDIR)
  test.must_contain(ninja_file, 'cflags = /O1')

  ninja_file = test.built_file_path('obj/test_opt_lev_speed.ninja', chdir=CHDIR)
  test.must_contain(ninja_file, 'cflags = /O2')

  ninja_file = test.built_file_path('obj/test_opt_lev_max.ninja', chdir=CHDIR)
  test.must_contain(ninja_file, 'cflags = /Ox')

  ninja_file = test.built_file_path('obj/test_opt_unset.ninja', chdir=CHDIR)
  test.must_not_contain(ninja_file, '/Od')
  test.must_not_contain(ninja_file, '/O1')
  test.must_not_contain(ninja_file, '/Ox')
  # Set by default if none specified.
  test.must_contain(ninja_file, '/O2')

  ninja_file = test.built_file_path('obj/test_opt_fpo.ninja', chdir=CHDIR)
  test.must_contain(ninja_file, '/Oy')
  test.must_not_contain(ninja_file, '/Oy-')

  ninja_file = test.built_file_path('obj/test_opt_fpo_off.ninja', chdir=CHDIR)
  test.must_contain(ninja_file, '/Oy-')

  ninja_file = test.built_file_path('obj/test_opt_intrinsic.ninja',
      chdir=CHDIR)
  test.must_contain(ninja_file, '/Oi')
  test.must_not_contain(ninja_file, '/Oi-')

  ninja_file = test.built_file_path('obj/test_opt_intrinsic_off.ninja',
      chdir=CHDIR)
  test.must_contain(ninja_file, '/Oi-')

  ninja_file = test.built_file_path('obj/test_opt_inline_off.ninja',
      chdir=CHDIR)
  test.must_contain(ninja_file, '/Ob0')

  ninja_file = test.built_file_path('obj/test_opt_inline_manual.ninja',
      chdir=CHDIR)
  test.must_contain(ninja_file, '/Ob1')

  ninja_file = test.built_file_path('obj/test_opt_inline_auto.ninja',
      chdir=CHDIR)
  test.must_contain(ninja_file, '/Ob2')

  ninja_file = test.built_file_path('obj/test_opt_neither.ninja',
      chdir=CHDIR)
  test.must_not_contain(ninja_file, '/Os')
  test.must_not_contain(ninja_file, '/Ot')

  ninja_file = test.built_file_path('obj/test_opt_size.ninja',
      chdir=CHDIR)
  test.must_contain(ninja_file, '/Os')

  ninja_file = test.built_file_path('obj/test_opt_speed.ninja',
      chdir=CHDIR)
  test.must_contain(ninja_file, '/Ot')

  ninja_file = test.built_file_path('obj/test_opt_wpo.ninja',
      chdir=CHDIR)
  test.must_contain(ninja_file, '/GL')

  ninja_file = test.built_file_path('obj/test_opt_sp.ninja',
      chdir=CHDIR)
  test.must_contain(ninja_file, '/GF')

  ninja_file = test.built_file_path('obj/test_opt_sp_off.ninja',
      chdir=CHDIR)
  test.must_not_contain(ninja_file, '/GF')

  ninja_file = test.built_file_path('obj/test_opt_fso.ninja',
      chdir=CHDIR)
  test.must_contain(ninja_file, '/GT')

  ninja_file = test.built_file_path('obj/test_opt_fso_off.ninja',
      chdir=CHDIR)
  test.must_not_contain(ninja_file, '/GT')

  test.pass_test()
