#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies 'AR' in make_global_settings.
"""

import os
import sys
import TestGyp

def verify_ar(tst, ars=None, rel_path=False, is_cross=True):
  ars = ars or {}
  rel_path = rel_path or set()
  for toolset in ['host', 'target']:
    ar = ars.get(toolset)
    ar_expected = fix_ar(ar, toolset in rel_path, tst)
    ar_suffix = '' if toolset == 'target' else 'host'
    if toolset == 'host':
      ar_expected = ar_expected or ('lib.exe' if sys.platform == 'win32' else 'ar')
    if tst.is_make:
      ar_suffix = ar_suffix and '.' + ar_suffix
      filename = 'Makefile'
      if toolset == 'target' and ar is None:
        tst.must_not_contain(filename, 'AR ?= ')
        return
      ar_rule = 'AR%s ?= %s' % (ar_suffix, ar_expected)
    elif tst.is_ninja:
      if not is_cross and toolset == 'host':
        return
      filename = 'out/Default/build.ninja'
      ar_suffix = ar_suffix and '_' + ar_suffix
      ar_expected = ar_expected or ('lib.exe' if sys.platform == 'win32' else 'ar')
      ar_rule = 'ar%s = %s' % (ar_suffix, ar_expected)
    else:
      tst.fail_test()
      return

    tst.must_contain(filename, ar_rule)


def fix_ar(ar, rel_path, tst):
  if rel_path:
    if ar is None:
      ar_expected = ''
    elif tst.is_make:
      ar_expected = '$(abspath %s)' % ar
    elif tst.is_ninja:
      ar_expected = os.path.join('..', '..', ar)
    else:
      tst.fail_test()
      return
  else:
    ar_expected = ar or ''
  return ar_expected


test_format = ['ninja']
if sys.platform.startswith('linux') or sys.platform == 'darwin':
  test_format += ['make']

test = TestGyp.TestGyp(formats=test_format)

# Check default values
test.run_gyp('make_global_settings_ar.gyp')
verify_ar(test, is_cross=False)


# Check default values with GYP_CROSSCOMPILE enabled.
with TestGyp.LocalEnv({'GYP_CROSSCOMPILE': '1'}):
  test.run_gyp('make_global_settings_ar.gyp')
verify_ar(test)


# Test 'AR' in 'make_global_settings'.
with TestGyp.LocalEnv({'GYP_CROSSCOMPILE': '1'}):
  test.run_gyp('make_global_settings_ar.gyp', '-Dcustom_ar_target=my_ar')
verify_ar(test, ars={'target': 'my_ar'}, rel_path={'target', 'host'})


# Test 'AR'/'AR.host' in 'make_global_settings'.
with TestGyp.LocalEnv({'GYP_CROSSCOMPILE': '1'}):
  test.run_gyp('make_global_settings_ar.gyp', '-Dcustom_ar_target=my_ar_target1', '-Dcustom_ar_host=my_ar_host1')
verify_ar(test, ars={'target': 'my_ar_target1', 'host': 'my_ar_host1'}, rel_path={'target', 'host'})


# Test $AR and $AR_host environment variables.
with TestGyp.LocalEnv({'AR': 'my_ar_target2', 'AR_host': 'my_ar_host2'}):
  test.run_gyp('make_global_settings_ar.gyp')
# Ninja generator resolves $AR in gyp phase. Make generator doesn't.
ar_target_expected = None
if test.is_ninja:
  ar_target_expected = 'lib.exe' if sys.platform == 'win32' else 'my_ar_target2'
verify_ar(test, ars={'target': ar_target_expected, 'host': 'my_ar_host2'})


# Test 'AR' in 'make_global_settings' with $AR_host environment variable.
with TestGyp.LocalEnv({'AR_host': 'my_ar_host3'}):
  test.run_gyp('make_global_settings_ar.gyp', '-Dcustom_ar_target=my_ar_target3')
verify_ar(test, ars={'target': 'my_ar_target3', 'host': 'my_ar_host3'}, rel_path={'target'})


test.pass_test()
