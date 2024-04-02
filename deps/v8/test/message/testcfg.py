# Copyright 2008 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import re

from testrunner.local import testsuite
from testrunner.local import utils
from testrunner.objects import testcase
from testrunner.outproc import message


INVALID_FLAGS = ["--enable-slow-asserts"]


class TestSuite(testsuite.TestSuite):
  def _test_loader_class(self):
    return testsuite.JSTestLoader

  def _test_class(self):
    return TestCase


class TestCase(testcase.D8TestCase):
  def __init__(self, *args, **kwargs):
    super(TestCase, self).__init__(*args, **kwargs)

    # get_source() relies on this being set.
    self._base_path = os.path.join(self.suite.root, self.path)
    source = self.get_source()
    self._source_files = self._parse_source_files(source)
    # Do not stress-opt message tests, since that changes the output.
    self._source_flags = ['--no-stress-opt'] + self._parse_source_flags(source)

  def _parse_source_files(self, source):
    files = []
    files.append(self._get_source_path())
    return files

  def _expected_fail(self):
    path = self.path
    while path:
      head, tail = os.path.split(path)
      if tail == 'fail':
        return True
      path = head
    return False

  def _get_cmd_params(self):
    params = super(TestCase, self)._get_cmd_params()
    return [p for p in params if p not in INVALID_FLAGS]

  def _get_files_params(self):
    return self._source_files

  def _get_source_flags(self):
    return self._source_flags

  def _get_source_path(self):
    # Try .js first, and fall back to .mjs.
    # TODO(v8:9406): clean this up by never separating the path from
    # the extension in the first place.
    if os.path.exists(self._base_path + self._get_suffix()):
      return self._base_path + self._get_suffix()
    return self._base_path + '.mjs'

  def skip_predictable(self):
    # Message tests expected to fail don't print allocation output for
    # predictable testing.
    return super(TestCase, self).skip_predictable() or self._expected_fail()

  @property
  def output_proc(self):
    return message.OutProc(self.expected_outcomes,
                           self._base_path,
                           self._expected_fail(),
                           self._base_path + '.out',
                           self.suite.test_config.regenerate_expected_files)


def GetSuite(*args, **kwargs):
  return TestSuite(*args, **kwargs)
