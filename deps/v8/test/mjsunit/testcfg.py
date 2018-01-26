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
from testrunner.objects import testcase
from testrunner.outproc import base as outproc

FILES_PATTERN = re.compile(r"//\s+Files:(.*)")
ENV_PATTERN = re.compile(r"//\s+Environment Variables:(.*)")
SELF_SCRIPT_PATTERN = re.compile(r"//\s+Env: TEST_FILE_NAME")
MODULE_PATTERN = re.compile(r"^// MODULE$", flags=re.MULTILINE)
NO_HARNESS_PATTERN = re.compile(r"^// NO HARNESS$", flags=re.MULTILINE)


class TestSuite(testsuite.TestSuite):
  def ListTests(self, context):
    tests = []
    for dirname, dirs, files in os.walk(self.root, followlinks=True):
      for dotted in [x for x in dirs if x.startswith('.')]:
        dirs.remove(dotted)
      dirs.sort()
      files.sort()
      for filename in files:
        if filename.endswith(".js") and filename != "mjsunit.js":
          fullpath = os.path.join(dirname, filename)
          relpath = fullpath[len(self.root) + 1 : -3]
          testname = relpath.replace(os.path.sep, "/")
          test = self._create_test(testname)
          tests.append(test)
    return tests

  def _test_combiner_class(self):
    return TestCombiner

  def _test_class(self):
    return TestCase


class TestCase(testcase.TestCase):
  def __init__(self, *args, **kwargs):
    super(TestCase, self).__init__(*args, **kwargs)

    source = self.get_source()

    files_list = []  # List of file names to append to command arguments.
    files_match = FILES_PATTERN.search(source);
    # Accept several lines of 'Files:'.
    while True:
      if files_match:
        files_list += files_match.group(1).strip().split()
        files_match = FILES_PATTERN.search(source, files_match.end())
      else:
        break
    files = [ os.path.normpath(os.path.join(self.suite.root, '..', '..', f))
              for f in files_list ]
    testfilename = os.path.join(self.suite.root,
                                self.path + self._get_suffix())
    if SELF_SCRIPT_PATTERN.search(source):
      files = (
        ["-e", "TEST_FILE_NAME=\"%s\"" % testfilename.replace("\\", "\\\\")] +
        files)

    if NO_HARNESS_PATTERN.search(source):
      mjsunit_files = []
    else:
      mjsunit_files = [os.path.join(self.suite.root, "mjsunit.js")]

    files_suffix = []
    if MODULE_PATTERN.search(source):
      files_suffix.append("--module")
    files_suffix.append(testfilename)

    self._source_files = files
    self._source_flags = self._parse_source_flags(source)
    self._mjsunit_files = mjsunit_files
    self._files_suffix = files_suffix
    self._env = self._parse_source_env(source)

  def _parse_source_env(self, source):
    env_match = ENV_PATTERN.search(source)
    env = {}
    if env_match:
      for env_pair in env_match.group(1).strip().split():
        var, value = env_pair.split('=')
        env[var] = value
    return env

  def _get_source_flags(self):
    return self._source_flags

  def _get_files_params(self, ctx):
    files = list(self._source_files)
    if not ctx.no_harness:
      files += self._mjsunit_files
    files += self._files_suffix
    if ctx.isolates:
      files += ['--isolate'] + files

    return files

  def _get_cmd_env(self):
    return self._env

  def _get_source_path(self):
    return os.path.join(self.suite.root, self.path + self._get_suffix())


class TestCombiner(testsuite.TestCombiner):
  def get_group_key(self, test):
    """Combine tests with the same set of flags.
    Ignore:
    1. Some special cases where it's not obvious what to pass in the command.
    2. Tests with flags that can cause failure even inside try-catch wrapper.
    3. Tests that use async functions. Async functions can be scheduled after
      exiting from try-catch wrapper and cause failure.
    """
    if (len(test._files_suffix) > 1 or
        test._env or
        not test._mjsunit_files or
        test._source_files):
      return None

    source_flags = test._get_source_flags()
    if ('--expose-trigger-failure' in source_flags or
        '--throws' in source_flags):
      return None

    source_code = test.get_source()
    # Maybe we could just update the tests to await all async functions they
    # call?
    if 'async' in source_code:
      return None

    # TODO(majeski): Investigate if we can maybe ignore the flags while
    # grouping.
    return str(sorted(list(set(source_flags + test._get_statusfile_flags()))))

  def _combined_test_class(self):
    return CombinedTest


class CombinedTest(testcase.TestCase):
  """Behaves like normal mjsunit tests except:
    1. Expected outcome is always PASS
    2. Instead of one file there is a try-catch wrapper with all combined tests
      passed as arguments.
  """
  def __init__(self, name, tests):
    super(CombinedTest, self).__init__(tests[0].suite, '', name)
    self._tests = tests

  def _prepare_outcomes(self, force_update=True):
    self._statusfile_outcomes = outproc.OUTCOMES_PASS
    self.expected_outcomes = outproc.OUTCOMES_PASS

  def _get_shell_with_flags(self, ctx):
    """In addition to standard set of shell flags it appends:
      --disable-abortjs: %AbortJS can abort the test even inside
        trycatch-wrapper, so we disable it.
      --quiet-load: suppress any stdout from load() function used by
        trycatch-wrapper.
    """
    shell = 'd8'
    shell_flags = ['--test', '--disable-abortjs', '--quiet-load']
    if ctx.random_seed:
      shell_flags.append('--random-seed=%s' % ctx.random_seed)
    return shell, shell_flags

  def _get_cmd_params(self, ctx):
    return (
      super(CombinedTest, self)._get_cmd_params(ctx) +
      self._tests[0]._mjsunit_files +
      ['tools/testrunner/trycatch_loader.js', '--'] +
      [t._files_suffix[0] for t in self._tests]
    )

  def _get_source_flags(self):
    return self._tests[0]._get_source_flags()

  def _get_statusfile_flags(self):
    return self._tests[0]._get_statusfile_flags()


def GetSuite(name, root):
  return TestSuite(name, root)
