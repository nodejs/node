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

def resolve_path(test, path):
  if path is None:
    return None
  elif test.format == 'make':
    return '$(abspath %s)' % path
  elif test.format in ['ninja', 'xcode-ninja']:
    return os.path.join('..', '..', path)
  else:
    test.fail_test()


def verify_ld_target(test, ld=None, rel_path=False):
  if rel_path:
    ld_expected = resolve_path(test, ld)
  else:
    ld_expected = ld
  # Resolve default values
  if ld_expected is None:
    if test.format == 'make':
      # Make generator hasn't set the default value for LD.
      # You can remove the following assertion as long as it doesn't
      # break existing projects.
      test.must_not_contain('Makefile', 'LD ?= ')
      return
    elif test.format in ['ninja', 'xcode-ninja']:
      if sys.platform == 'win32':
        ld_expected = 'link.exe'
      else:
        ld_expected = '$cc'
  if test.format == 'make':
    test.must_contain('Makefile', 'LD ?= %s' % ld_expected)
  elif test.format in ['ninja', 'xcode-ninja']:
    test.must_contain('out/Default/build.ninja', 'ld = %s' % ld_expected)
  else:
    test.fail_test()


def verify_ld_host(test, ld=None, rel_path=False):
  if rel_path:
    ld_expected = resolve_path(test, ld)
  else:
    ld_expected = ld
  # Resolve default values
  if ld_expected is None:
    if test.format == 'make':
      # Make generator hasn't set the default value for LD.host.
      # You can remove the following assertion as long as it doesn't
      # break existing projects.
      test.must_not_contain('Makefile', 'LD.host ?= ')
      return
    elif test.format in ['ninja', 'xcode-ninja']:
      if sys.platform == 'win32':
        ld_expected = '$ld'
      else:
        ld_expected = '$cc_host'
  if test.format == 'make':
    test.must_contain('Makefile', 'LD.host ?= %s' % ld_expected)
  elif test.format in ['ninja', 'xcode-ninja']:
    test.must_contain('out/Default/build.ninja', 'ld_host = %s' % ld_expected)
  else:
    test.fail_test()


test_format = ['ninja']
if sys.platform in ('linux2', 'darwin'):
  test_format += ['make']

test = TestGyp.TestGyp(formats=test_format)

# Check default values
test.run_gyp('make_global_settings_ld.gyp')
verify_ld_target(test)


# Check default values with GYP_CROSSCOMPILE enabled.
with TestGyp.LocalEnv({'GYP_CROSSCOMPILE': '1'}):
  test.run_gyp('make_global_settings_ld.gyp')
verify_ld_target(test)
verify_ld_host(test)


# Test 'LD' in 'make_global_settings'.
with TestGyp.LocalEnv({'GYP_CROSSCOMPILE': '1'}):
  test.run_gyp('make_global_settings_ld.gyp', '-Dcustom_ld_target=my_ld')
verify_ld_target(test, ld='my_ld', rel_path=True)


# Test 'LD'/'LD.host' in 'make_global_settings'.
with TestGyp.LocalEnv({'GYP_CROSSCOMPILE': '1'}):
  test.run_gyp('make_global_settings_ld.gyp',
               '-Dcustom_ld_target=my_ld_target1',
               '-Dcustom_ld_host=my_ld_host1')
verify_ld_target(test, ld='my_ld_target1', rel_path=True)
verify_ld_host(test, ld='my_ld_host1', rel_path=True)


# Unlike other environment variables such as $AR/$AR_host, $CC/$CC_host,
# and $CXX/$CXX_host, neither Make generator nor Ninja generator recognizes
# $LD/$LD_host environment variables as of r1935. This may or may not be
# intentional, but here we leave a test case to verify this behavior just for
# the record.
# If you want to support $LD/$LD_host, please revise the following test case as
# well as the generator.
with TestGyp.LocalEnv({'GYP_CROSSCOMPILE': '1',
                       'LD': 'my_ld_target2',
                       'LD_host': 'my_ld_host2'}):
  test.run_gyp('make_global_settings_ld.gyp')
if test.format == 'make':
  test.must_not_contain('Makefile', 'my_ld_target2')
  test.must_not_contain('Makefile', 'my_ld_host2')
elif test.format == 'ninja':
  test.must_not_contain('out/Default/build.ninja', 'my_ld_target2')
  test.must_not_contain('out/Default/build.ninja', 'my_ld_host2')


test.pass_test()
