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

# for py2/py3 compatibility
from __future__ import print_function

import os
import shutil

from testrunner.local import command
from testrunner.local import testsuite
from testrunner.local import utils
from testrunner.objects import testcase

SHELL = 'cctest'


class TestLoader(testsuite.TestLoader):
  def _list_test_filenames(self):
    shell = os.path.abspath(os.path.join(self.test_config.shell_dir, SHELL))
    if utils.IsWindows():
      shell += ".exe"
    cmd = self.ctx.command(
        cmd_prefix=self.test_config.command_prefix,
        shell=shell,
        args=["--list"] + self.test_config.extra_flags)
    output = cmd.execute()
    # TODO make errors visible (see duplicated code in 'unittests')
    if output.exit_code != 0:
      print(cmd)
      print(output.stdout)
      print(output.stderr)
      return []

    filtered_output = []
    test_prefix = '**>Test: '
    test_total_prefix = 'Total number of tests: '
    tests_total = 0

    for line in output.stdout.strip().splitlines():
      if line.startswith(test_prefix):
        filtered_output.append(line[len(test_prefix):])
      if line.startswith(test_total_prefix):
        tests_total = int(line[len(test_total_prefix):])

    assert (len(filtered_output) > 0)
    assert (len(filtered_output) == tests_total)

    return sorted(filtered_output)


class TestSuite(testsuite.TestSuite):
  def _test_loader_class(self):
    return TestLoader

  def _test_class(self):
    return TestCase


class TestCase(testcase.TestCase):
  def get_shell(self):
    return SHELL

  def _get_files_params(self):
    return [self.path]
