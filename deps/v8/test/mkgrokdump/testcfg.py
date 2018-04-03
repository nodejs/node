# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from testrunner.local import testsuite
from testrunner.objects import testcase
from testrunner.outproc import mkgrokdump


SHELL = 'mkgrokdump'

class TestSuite(testsuite.TestSuite):
  def __init__(self, *args, **kwargs):
    super(TestSuite, self).__init__(*args, **kwargs)

    v8_path = os.path.dirname(os.path.dirname(os.path.abspath(self.root)))
    self.expected_path = os.path.join(v8_path, 'tools', 'v8heapconst.py')

  def ListTests(self, context):
    test = self._create_test(SHELL)
    return [test]

  def _test_class(self):
    return TestCase


class TestCase(testcase.TestCase):
  def _get_variant_flags(self):
    return []

  def _get_statusfile_flags(self):
    return []

  def _get_mode_flags(self, ctx):
    return []

  def get_shell(self):
    return SHELL

  @property
  def output_proc(self):
    return mkgrokdump.OutProc(self.expected_outcomes, self.suite.expected_path)


def GetSuite(name, root):
  return TestSuite(name, root)
