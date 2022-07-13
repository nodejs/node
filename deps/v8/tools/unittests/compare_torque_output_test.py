#!/usr/bin/env python3
# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import subprocess
import sys
import tempfile
import unittest

TOOLS_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
COMPARE_SCRIPT = os.path.join(TOOLS_DIR, 'compare_torque_output.py')
TEST_DATA = os.path.join(TOOLS_DIR, 'unittests', 'testdata', 'compare_torque')


class PredictableTest(unittest.TestCase):
  def setUp(self):
    fd, self.tmp_file  = tempfile.mkstemp()
    os.close(fd)

  def _compare_from(self, test_folder):
    file1 = os.path.join(TEST_DATA, test_folder, 'f1')
    file2 = os.path.join(TEST_DATA, test_folder, 'f2')
    proc = subprocess.Popen([
          sys.executable, '-u',
          COMPARE_SCRIPT, file1, file2, self.tmp_file
        ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    _, err = proc.communicate()
    return proc.returncode, err

  def test_content_diff(self):
    exitcode, output = self._compare_from('test1')
    self.assertEqual(1, exitcode)
    full_match = r'^Found.*-line 2\+line 2 with diff.*\+line 3\n\n$'
    self.assertRegex(output.decode('utf-8'), re.compile(full_match, re.M | re.S))

  def test_no_diff(self):
    exitcode, output = self._compare_from('test2')
    self.assertEqual(0, exitcode)
    self.assertFalse(output)

  def test_right_only(self):
    exitcode, output = self._compare_from('test3')
    self.assertEqual(1, exitcode)
    self.assertRegex(output.decode('utf-8'), r'Some files exist only in.*f2\nfile3')

  def test_left_only(self):
    exitcode, output = self._compare_from('test4')
    self.assertEqual(1, exitcode)
    self.assertRegex(output.decode('utf-8'), r'Some files exist only in.*f1\nfile4')

  def tearDown(self):
    os.unlink(self.tmp_file)
