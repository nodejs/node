#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies things related to bundle resources.
"""

import TestGyp

import os
import stat
import sys


def check_attribs(path, expected_exec_bit):
  out_path = test.built_file_path(
      os.path.join('resource.app/Contents/Resources', path), chdir=CHDIR)

  in_stat = os.stat(os.path.join(CHDIR, path))
  out_stat = os.stat(out_path)
  if in_stat.st_mtime == out_stat.st_mtime:
    test.fail_test()
  if out_stat.st_mode & stat.S_IXUSR != expected_exec_bit:
    test.fail_test()


if sys.platform == 'darwin':
  # set |match| to ignore build stderr output.
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  CHDIR = 'bundle-resources'
  test.run_gyp('test.gyp', chdir=CHDIR)

  test.build('test.gyp', test.ALL, chdir=CHDIR)

  test.built_file_must_match('resource.app/Contents/Resources/secret.txt',
                             'abc\n', chdir=CHDIR)
  test.built_file_must_match('source_rule.app/Contents/Resources/secret.txt',
                             'ABC\n', chdir=CHDIR)

  test.built_file_must_match(
      'resource.app/Contents/Resources/executable-file.sh',
      '#!/bin/bash\n'
      '\n'
      'echo echo echo echo cho ho o o\n', chdir=CHDIR)

  check_attribs('executable-file.sh', expected_exec_bit=stat.S_IXUSR)
  check_attribs('secret.txt', expected_exec_bit=0)

  # TODO(thakis): This currently fails with make.
  if test.format != 'make':
    test.built_file_must_match(
        'resource_rule.app/Contents/Resources/secret.txt', 'ABC\n', chdir=CHDIR)

  test.pass_test()
