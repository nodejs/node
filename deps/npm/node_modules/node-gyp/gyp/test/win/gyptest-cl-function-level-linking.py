#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure function-level linking setting is extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'compiler-flags'
  test.run_gyp('function-level-linking.gyp', chdir=CHDIR)
  test.build('function-level-linking.gyp', test.ALL, chdir=CHDIR)

  def CheckForSectionString(binary, search_for, should_exist):
    output = test.run_dumpbin('/headers', binary)
    if should_exist and search_for not in output:
      print 'Did not find "%s" in %s' % (search_for, binary)
      test.fail_test()
    elif not should_exist and search_for in output:
      print 'Found "%s" in %s (and shouldn\'t have)' % (search_for, binary)
      test.fail_test()

  def Object(proj, obj):
    sep = '.' if test.format == 'ninja' else '\\'
    return 'obj\\%s%s%s' % (proj, sep, obj)

  look_for = '''COMDAT; sym= "int __cdecl comdat_function'''

  # When function level linking is on, the functions should be listed as
  # separate comdat entries.

  CheckForSectionString(
      test.built_file_path(Object('test_fll_on', 'function-level-linking.obj'),
                           chdir=CHDIR),
      look_for,
      should_exist=True)

  CheckForSectionString(
      test.built_file_path(Object('test_fll_off', 'function-level-linking.obj'),
                           chdir=CHDIR),
      look_for,
      should_exist=False)

  test.pass_test()
