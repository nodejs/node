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

def resolve_path(test, path):
  if path is None:
    return None
  elif test.format == 'make':
    return '$(abspath %s)' % path
  elif test.format in ['ninja', 'xcode-ninja']:
    return os.path.join('..', '..', path)
  else:
    test.fail_test()


def verify_ar_target(test, ar=None, rel_path=False):
  if rel_path:
    ar_expected = resolve_path(test, ar)
  else:
    ar_expected = ar
  # Resolve default values
  if ar_expected is None:
    if test.format == 'make':
      # Make generator hasn't set the default value for AR.
      # You can remove the following assertion as long as it doesn't
      # break existing projects.
      test.must_not_contain('Makefile', 'AR ?= ')
      return
    elif test.format in ['ninja', 'xcode-ninja']:
      if sys.platform == 'win32':
        ar_expected = 'lib.exe'
      else:
        ar_expected = 'ar'
  if test.format == 'make':
    test.must_contain('Makefile', 'AR ?= %s' % ar_expected)
  elif test.format in ['ninja', 'xcode-ninja']:
    test.must_contain('out/Default/build.ninja', 'ar = %s' % ar_expected)
  else:
    test.fail_test()


def verify_ar_host(test, ar=None, rel_path=False):
  if rel_path:
    ar_expected = resolve_path(test, ar)
  else:
    ar_expected = ar
  # Resolve default values
  if ar_expected is None:
    ar_expected = 'ar'
  if test.format == 'make':
    test.must_contain('Makefile', 'AR.host ?= %s' % ar_expected)
  elif test.format in ['ninja', 'xcode-ninja']:
    test.must_contain('out/Default/build.ninja', 'ar_host = %s' % ar_expected)
  else:
    test.fail_test()


test_format = ['ninja']
if sys.platform in ('linux2', 'darwin'):
  test_format += ['make']

test = TestGyp.TestGyp(formats=test_format)

# Check default values
test.run_gyp('make_global_settings_ar.gyp')
verify_ar_target(test)


# Check default values with GYP_CROSSCOMPILE enabled.
with TestGyp.LocalEnv({'GYP_CROSSCOMPILE': '1'}):
  test.run_gyp('make_global_settings_ar.gyp')
verify_ar_target(test)
verify_ar_host(test)


# Test 'AR' in 'make_global_settings'.
with TestGyp.LocalEnv({'GYP_CROSSCOMPILE': '1'}):
  test.run_gyp('make_global_settings_ar.gyp', '-Dcustom_ar_target=my_ar')
verify_ar_target(test, ar='my_ar', rel_path=True)


# Test 'AR'/'AR.host' in 'make_global_settings'.
with TestGyp.LocalEnv({'GYP_CROSSCOMPILE': '1'}):
  test.run_gyp('make_global_settings_ar.gyp',
               '-Dcustom_ar_target=my_ar_target1',
               '-Dcustom_ar_host=my_ar_host1')
verify_ar_target(test, ar='my_ar_target1', rel_path=True)
verify_ar_host(test, ar='my_ar_host1', rel_path=True)


# Test $AR and $AR_host environment variables.
with TestGyp.LocalEnv({'AR': 'my_ar_target2',
                       'AR_host': 'my_ar_host2'}):
  test.run_gyp('make_global_settings_ar.gyp')
# Ninja generator resolves $AR in gyp phase. Make generator doesn't.
if test.format == 'ninja':
  if sys.platform == 'win32':
    # TODO(yukawa): Make sure if this is an expected result or not.
    verify_ar_target(test, ar='lib.exe', rel_path=False)
  else:
    verify_ar_target(test, ar='my_ar_target2', rel_path=False)
verify_ar_host(test, ar='my_ar_host2', rel_path=False)


# Test 'AR' in 'make_global_settings' with $AR_host environment variable.
with TestGyp.LocalEnv({'AR_host': 'my_ar_host3'}):
  test.run_gyp('make_global_settings_ar.gyp',
               '-Dcustom_ar_target=my_ar_target3')
verify_ar_target(test, ar='my_ar_target3', rel_path=True)
verify_ar_host(test, ar='my_ar_host3', rel_path=False)


test.pass_test()
