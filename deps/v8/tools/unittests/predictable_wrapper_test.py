#!/usr/bin/env python3
# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys
import tempfile
import unittest

TOOLS_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

PREDICTABLE_WRAPPER = os.path.join(
    TOOLS_DIR, 'predictable_wrapper.py')

PREDICTABLE_MOCKED = os.path.join(
    TOOLS_DIR, 'unittests', 'testdata', 'predictable_mocked.py')

def call_wrapper(mode):
  """Call the predictable wrapper under test with a mocked file to test.

  Instead of d8, we use python and a python mock script. This mock script is
  expecting two arguments, mode (one of 'equal', 'differ' or 'missing') and
  a path to a temporary file for simulating non-determinism.
  """
  fd, state_file = tempfile.mkstemp()
  os.close(fd)
  try:
    args = [
      sys.executable,
      PREDICTABLE_WRAPPER,
      sys.executable,
      PREDICTABLE_MOCKED,
      mode,
      state_file,
    ]
    proc = subprocess.Popen(args, stdout=subprocess.PIPE)
    proc.communicate()
    return proc.returncode
  finally:
    os.unlink(state_file)


class PredictableTest(unittest.TestCase):
  def testEqualAllocationOutput(self):
    self.assertEqual(0, call_wrapper('equal'))

  def testNoAllocationOutput(self):
    self.assertEqual(2, call_wrapper('missing'))

  def testDifferentAllocationOutput(self):
    self.assertEqual(3, call_wrapper('differ'))


if __name__ == '__main__':
  unittest.main()
