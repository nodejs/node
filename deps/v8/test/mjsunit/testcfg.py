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

from collections import OrderedDict
import itertools
import os
import re

from testrunner.local import statusfile
from testrunner.local import testsuite
from testrunner.objects import testcase
from testrunner.outproc import base as outproc


FILES_PATTERN = re.compile(r"//\s+Files:(.*)")
ENV_PATTERN = re.compile(r"//\s+Environment Variables:(.*)")
SELF_SCRIPT_PATTERN = re.compile(r"//\s+Env: TEST_FILE_NAME")
NO_HARNESS_PATTERN = re.compile(r"^// NO HARNESS$", flags=re.MULTILINE)


# Flags known to misbehave when combining arbitrary mjsunit tests. Can also
# be compiled regular expressions.
MISBEHAVING_COMBINED_TESTS_FLAGS= [
  '--check-handle-count',
  '--enable-tracing',
  re.compile('--experimental.*'),
  '--expose-trigger-failure',
  re.compile('--harmony.*'),
  '--mock-arraybuffer-allocator',
  '--print-ast',
  re.compile('--trace.*'),
  '--wasm-lazy-compilation',
]


class TestLoader(testsuite.JSTestLoader):
  @property
  def excluded_files(self):
    return {
      "mjsunit.js",
      "mjsunit_numfuzz.js",
    }


class TestSuite(testsuite.TestSuite):
  def _test_loader_class(self):
    return TestLoader

  def _test_combiner_class(self):
    return TestCombiner

  def _test_class(self):
    return TestCase


class TestCase(testcase.D8TestCase):
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
    testfilename = self._get_source_path()
    if SELF_SCRIPT_PATTERN.search(source):
      files = (
        ["-e", "TEST_FILE_NAME=\"%s\"" % testfilename.replace("\\", "\\\\")] +
        files)

    if NO_HARNESS_PATTERN.search(source):
      mjsunit_files = []
    else:
      mjsunit_files = [os.path.join(self.suite.root, "mjsunit.js")]

    if self.suite.framework_name == 'num_fuzzer':
      mjsunit_files.append(os.path.join(self.suite.root, "mjsunit_numfuzz.js"))

    self._source_files = files
    self._source_flags = self._parse_source_flags(source)
    self._mjsunit_files = mjsunit_files
    self._files_suffix = [testfilename]
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

  def _get_files_params(self):
    files = list(self._source_files)
    if not self._test_config.no_harness:
      files += self._mjsunit_files
    files += self._files_suffix
    if self._test_config.isolates:
      files += ['--isolate'] + files

    return files

  def _get_cmd_env(self):
    return self._env

  def _get_source_path(self):
    base_path = os.path.join(self.suite.root, self.path)
    # Try .js first, and fall back to .mjs.
    # TODO(v8:9406): clean this up by never separating the path from
    # the extension in the first place.
    if os.path.exists(base_path + self._get_suffix()):
      return base_path + self._get_suffix()
    return base_path + '.mjs'


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

    # TODO(machenbach): Remove grouping if combining tests in a flag-independent
    # way works well.
    return 1

  def _combined_test_class(self):
    return CombinedTest


class CombinedTest(testcase.D8TestCase):
  """Behaves like normal mjsunit tests except:
    1. Expected outcome is always PASS
    2. Instead of one file there is a try-catch wrapper with all combined tests
      passed as arguments.
  """
  def __init__(self, name, tests):
    super(CombinedTest, self).__init__(tests[0].suite, '', name,
                                       tests[0]._test_config)
    self._tests = tests

  def _prepare_outcomes(self, force_update=True):
    self._statusfile_outcomes = outproc.OUTCOMES_PASS_OR_TIMEOUT
    self.expected_outcomes = outproc.OUTCOMES_PASS_OR_TIMEOUT

  def _get_shell_flags(self):
    """In addition to standard set of shell flags it appends:
      --disable-abortjs: %AbortJS can abort the test even inside
        trycatch-wrapper, so we disable it.
      --harmony: We skip all harmony flags due to false positives,
          but always pass the staging flag to cover the mature features.
      --omit-quit: Calling quit() in JS would otherwise early terminate.
      --quiet-load: suppress any stdout from load() function used by
        trycatch-wrapper.
    """
    return [
        '--test',
        '--disable-abortjs',
        '--harmony',
        '--omit-quit',
        '--quiet-load',
    ]

  def _get_cmd_params(self):
    return (
      super(CombinedTest, self)._get_cmd_params() +
      ['tools/testrunner/trycatch_loader.js', '--'] +
      self._tests[0]._mjsunit_files +
      ['--'] +
      [t._files_suffix[0] for t in self._tests]
    )

  def _merge_flags(self, flags):
    """Merges flags from a list of flags.

    Flag values not starting with '-' are merged with the preceeding flag,
    e.g. --foo 1 will become --foo=1. All other flags remain the same.

    Returns: A generator of flags.
    """
    if not flags:
      return
    # Iterate over flag pairs. ['-'] is a sentinel value for the last iteration.
    for flag1, flag2 in itertools.izip(flags, flags[1:] + ['-']):
      if not flag2.startswith('-'):
        assert '=' not in flag1
        yield flag1 + '=' + flag2
      elif flag1.startswith('-'):
        yield flag1

  def _is_flag_blocked(self, flag):
    for item in MISBEHAVING_COMBINED_TESTS_FLAGS:
      if isinstance(item, str):
        if item == flag:
          return True
      elif item.match(flag):
        return True
    return False

  def _get_combined_flags(self, flags_gen):
    """Combines all flags - dedupes, keeps order and filters some flags.

    Args:
      flags_gen: Generator for flag lists.
    Returns: A list of flags.
    """
    merged_flags = self._merge_flags(list(itertools.chain(*flags_gen)))
    unique_flags = OrderedDict((flag, True) for flag in merged_flags).keys()
    return [
      flag for flag in unique_flags
      if not self._is_flag_blocked(flag)
    ]

  def _get_source_flags(self):
    # Combine flags from all source files.
    return self._get_combined_flags(
        test._get_source_flags() for test in self._tests)

  def _get_statusfile_flags(self):
    # Combine flags from all status file entries.
    return self._get_combined_flags(
        test._get_statusfile_flags() for test in self._tests)
