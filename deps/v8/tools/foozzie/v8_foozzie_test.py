# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys
import unittest

import v8_foozzie
import v8_suppressions

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
FOOZZIE = os.path.join(BASE_DIR, 'v8_foozzie.py')
TEST_DATA = os.path.join(BASE_DIR, 'testdata')

class UnitTest(unittest.TestCase):
  def testDiff(self):
    # TODO(machenbach): Mock out suppression configuration.
    suppress = v8_suppressions.get_suppression(
        'x64', 'fullcode', 'x64', 'default')
    one = ''
    two = ''
    diff = None, None
    self.assertEquals(diff, suppress.diff(one, two))

    one = 'a \n  b\nc();'
    two = 'a \n  b\nc();'
    diff = None, None
    self.assertEquals(diff, suppress.diff(one, two))

    # Ignore line before caret, caret position, stack trace char numbers
    # error message and validator output.
    one = """
undefined
weird stuff
      ^
Validation of asm.js module failed: foo bar
somefile.js: TypeError: undefined is not a function
stack line :15: foo
  undefined
"""
    two = """
undefined
other weird stuff
            ^
somefile.js: TypeError: baz is not a function
stack line :2: foo
Validation of asm.js module failed: baz
  undefined
"""
    diff = None, None
    self.assertEquals(diff, suppress.diff(one, two))

    one = """
Still equal
Extra line
"""
    two = """
Still equal
"""
    diff = '- Extra line', None
    self.assertEquals(diff, suppress.diff(one, two))

    one = """
Still equal
"""
    two = """
Still equal
Extra line
"""
    diff = '+ Extra line', None
    self.assertEquals(diff, suppress.diff(one, two))

    one = """
undefined
somefile.js: TypeError: undefined is not a constructor
"""
    two = """
undefined
otherfile.js: TypeError: undefined is not a constructor
"""
    diff = """- somefile.js: TypeError: undefined is not a constructor
+ otherfile.js: TypeError: undefined is not a constructor""", None
    self.assertEquals(diff, suppress.diff(one, two))


def run_foozzie(first_d8, second_d8):
  return subprocess.check_output([
    sys.executable, FOOZZIE,
    '--random-seed', '12345',
    '--first-d8', os.path.join(TEST_DATA, first_d8),
    '--second-d8', os.path.join(TEST_DATA, second_d8),
    '--second-config', 'ignition_staging',
    os.path.join(TEST_DATA, 'fuzz-123.js'),
  ])


class SystemTest(unittest.TestCase):
  def testSyntaxErrorDiffPass(self):
    stdout = run_foozzie('test_d8_1.py', 'test_d8_2.py')
    self.assertEquals('# V8 correctness - pass\n', stdout)

  def testDifferentOutputFail(self):
    with open(os.path.join(TEST_DATA, 'failure_output.txt')) as f:
      expected_output = f.read()
    with self.assertRaises(subprocess.CalledProcessError) as ctx:
      run_foozzie('test_d8_1.py', 'test_d8_3.py')
    e = ctx.exception
    self.assertEquals(v8_foozzie.RETURN_FAIL, e.returncode)
    self.assertEquals(expected_output, e.output)
