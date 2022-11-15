#!/usr/bin/env python3
# Copyright 2022 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from pathlib import Path

import collections
import os
import unittest

import gcmole

TESTDATA_PATH = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), 'testdata', 'v8')

Options = collections.namedtuple('Options', ['v8_root_dir', 'v8_target_cpu'])


def abs_test_file(f):
  return Path(os.path.join(TESTDATA_PATH, f))


class FilesTest(unittest.TestCase):

  def testFileList_for_testing(self):
    options = Options(Path(TESTDATA_PATH), 'x64')
    self.assertEqual(
        gcmole.build_file_list(options, True),
        list(map(abs_test_file, ['tools/gcmole/gcmole-test.cc'])))

  def testFileList_x64(self):
    options = Options(Path(TESTDATA_PATH), 'x64')
    expected = [
        'file1.cc',
        'file2.cc',
        'x64/file1.cc',
        'x64/file2.cc',
        'file3.cc',
        'file4.cc',
        'test/cctest/test-x64-file1.cc',
        'test/cctest/test-x64-file2.cc',
    ]
    self.assertEqual(
        gcmole.build_file_list(options, False),
        list(map(abs_test_file, expected)))

  def testFileList_arm(self):
    options = Options(Path(TESTDATA_PATH), 'arm')
    expected = [
        'file1.cc',
        'file2.cc',
        'file3.cc',
        'file4.cc',
        'arm/file1.cc',
        'arm/file2.cc',
    ]
    self.assertEqual(
        gcmole.build_file_list(options, False),
        list(map(abs_test_file, expected)))


if __name__ == '__main__':
  unittest.main()
