#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure PGO is working properly.
"""

import TestGyp

import os
import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'linker-flags'
  test.run_gyp('pgo.gyp', chdir=CHDIR)

  def IsPGOAvailable():
    """Returns true if the Visual Studio available here supports PGO."""
    test.build('pgo.gyp', 'gen_linker_option', chdir=CHDIR)
    tmpfile = test.read(test.built_file_path('linker_options.txt', chdir=CHDIR))
    return any(line.find('PGOPTIMIZE') for line in tmpfile)

  # Test generated build files look fine.
  if test.format == 'ninja':
    ninja = test.built_file_path('obj/test_pgo_instrument.ninja', chdir=CHDIR)
    test.must_contain(ninja, '/LTCG:PGINSTRUMENT')
    test.must_contain(ninja, 'test_pgo.pgd')
    ninja = test.built_file_path('obj/test_pgo_optimize.ninja', chdir=CHDIR)
    test.must_contain(ninja, '/LTCG:PGOPTIMIZE')
    test.must_contain(ninja, 'test_pgo.pgd')
    ninja = test.built_file_path('obj/test_pgo_update.ninja', chdir=CHDIR)
    test.must_contain(ninja, '/LTCG:PGUPDATE')
    test.must_contain(ninja, 'test_pgo.pgd')
  elif test.format == 'msvs':
    LTCG_FORMAT = '<LinkTimeCodeGeneration>%s</LinkTimeCodeGeneration>'
    vcproj = test.workpath('linker-flags/test_pgo_instrument.vcxproj')
    test.must_contain(vcproj, LTCG_FORMAT % 'PGInstrument')
    test.must_contain(vcproj, 'test_pgo.pgd')
    vcproj = test.workpath('linker-flags/test_pgo_optimize.vcxproj')
    test.must_contain(vcproj, LTCG_FORMAT % 'PGOptimization')
    test.must_contain(vcproj, 'test_pgo.pgd')
    vcproj = test.workpath('linker-flags/test_pgo_update.vcxproj')
    test.must_contain(vcproj, LTCG_FORMAT % 'PGUpdate')
    test.must_contain(vcproj, 'test_pgo.pgd')

  # When PGO is available, try building binaries with PGO.
  if IsPGOAvailable():
    pgd_path = test.built_file_path('test_pgo.pgd', chdir=CHDIR)

    # Test if 'PGInstrument' generates PGD (Profile-Guided Database) file.
    if os.path.exists(pgd_path):
      test.unlink(pgd_path)
    test.must_not_exist(pgd_path)
    test.build('pgo.gyp', 'test_pgo_instrument', chdir=CHDIR)
    test.must_exist(pgd_path)

    # Test if 'PGOptimize' works well
    test.build('pgo.gyp', 'test_pgo_optimize', chdir=CHDIR)
    test.must_contain_any_line(test.stdout(), ['profiled functions'])

    # Test if 'PGUpdate' works well
    test.build('pgo.gyp', 'test_pgo_update', chdir=CHDIR)
    # With 'PGUpdate', linker should not complain that sources are changed after
    # the previous training run.
    test.touch(test.workpath('linker-flags/inline_test_main.cc'))
    test.unlink(test.built_file_path('test_pgo_update.exe', chdir=CHDIR))
    test.build('pgo.gyp', 'test_pgo_update', chdir=CHDIR)
    test.must_contain_any_line(test.stdout(), ['profiled functions'])

  test.pass_test()
