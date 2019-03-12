#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies 'LD' in make_global_settings.
"""

import os
import sys
import TestGyp

def verify_ld(tst, ld=None, rel_path=False, is_target=False):
  if rel_path:
    if ld is None:
      ld_expected = None
    elif tst.is_make:
      ld_expected = '$(abspath %s)' % ld
    elif tst.is_ninja:
      ld_expected = os.path.join('..', '..', ld)
    else:
      tst.fail_test()
      return
  else:
    ld_expected = ld

  ld_make_suffix = ''
  ld_ninja_suffix = ''
  if not is_target:
    ld_make_suffix = '.host'
    ld_ninja_suffix = '_host'

  if tst.is_make:
    # Make generator hasn't set the default value for AR.
    # You can remove the following assertion as long as it doesn't
    # break existing projects.
    if ld_expected is None:
      tst.must_not_contain('Makefile', 'LD%s ?= ' % ld_make_suffix)
    else:
      tst.must_contain('Makefile', 'LD%s ?= %s' % (ld_make_suffix, ld_expected))
  elif tst.is_ninja:
    if sys.platform == 'win32':
      ld_expected = ld_expected or ('link.exe' if is_target else '$ld')
    else:
      ld_expected = ld_expected or '$cc%s' % ld_ninja_suffix
    tst.must_contain('out/Default/build.ninja', 'ld%s = %s' % (ld_ninja_suffix, ld_expected))
  else:
    tst.fail_test()


test_format = ['ninja']
if sys.platform.startswith('linux') or sys.platform == 'darwin':
  test_format += ['make']

test = TestGyp.TestGyp(formats=test_format)

# Check default values
test.run_gyp('make_global_settings_ld.gyp')
verify_ld(test, is_target=True)


# Check default values with GYP_CROSSCOMPILE enabled.
with TestGyp.LocalEnv({'GYP_CROSSCOMPILE': '1'}):
  test.run_gyp('make_global_settings_ld.gyp')
verify_ld(test, is_target=True)
verify_ld(test)


# Test 'LD' in 'make_global_settings'.
with TestGyp.LocalEnv({'GYP_CROSSCOMPILE': '1'}):
  test.run_gyp('make_global_settings_ld.gyp', '-Dcustom_ld_target=my_ld')
verify_ld(test, ld='my_ld', rel_path=True, is_target=True)


# Test 'LD'/'LD.host' in 'make_global_settings'.
with TestGyp.LocalEnv({'GYP_CROSSCOMPILE': '1'}):
  test.run_gyp('make_global_settings_ld.gyp', '-Dcustom_ld_target=my_ld_target1', '-Dcustom_ld_host=my_ld_host1')
verify_ld(test, ld='my_ld_target1', rel_path=True, is_target=True)
verify_ld(test, ld='my_ld_host1', rel_path=True)


# Unlike other environment variables such as $AR/$AR_host, $CC/$CC_host,
# and $CXX/$CXX_host, neither Make generator nor Ninja generator recognizes
# $LD/$LD_host environment variables as of r1935. This may or may not be
# intentional, but here we leave a test case to verify this behavior just for
# the record.
# If you want to support $LD/$LD_host, please revise the following test case as
# well as the generator.
with TestGyp.LocalEnv({'GYP_CROSSCOMPILE': '1', 'LD': 'my_ld_target2', 'LD_host': 'my_ld_host2'}):
  test.run_gyp('make_global_settings_ld.gyp')
if test.is_make:
  test.must_not_contain('Makefile', 'my_ld_target2')
  test.must_not_contain('Makefile', 'my_ld_host2')
elif test.is_ninja:
  test.must_not_contain('out/Default/build.ninja', 'my_ld_target2')
  test.must_not_contain('out/Default/build.ninja', 'my_ld_host2')


test.pass_test()
