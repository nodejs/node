# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from testrunner.local import testsuite
from testrunner.objects import testcase
from testrunner.outproc import mkgrokdump


SHELL = 'mkgrokdump'


class TestLoader(testsuite.TestLoader):
  def _list_test_filenames(self):
    yield SHELL

#TODO(tmrts): refactor the test creation logic to migrate to TestLoader
class TestSuite(testsuite.TestSuite):

  def __init__(self, ctx, *args, **kwargs):
    super(TestSuite, self).__init__(ctx, *args, **kwargs)

  def _test_loader_class(self):
    return TestLoader

  def _test_class(self):
    return TestCase


class TestCase(testcase.TestCase):
  def _get_variant_flags(self):
    return []

  def _get_statusfile_flags(self):
    return []

  def _get_mode_flags(self):
    return []

  def get_shell(self):
    return SHELL

  @property
  def output_proc(self):
    return mkgrokdump.OutProc(self.expected_outcomes)
