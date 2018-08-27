# Copyright 2012 the V8 project authors. All rights reserved.
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

import copy
import os
import re
import shlex

from ..outproc import base as outproc
from ..local import command
from ..local import statusfile
from ..local import utils

FLAGS_PATTERN = re.compile(r"//\s+Flags:(.*)")



class TestCase(object):
  def __init__(self, suite, path, name, test_config):
    self.suite = suite        # TestSuite object

    self.path = path          # string, e.g. 'div-mod', 'test-api/foo'
    self.name = name          # string that identifies test in the status file

    self.variant = None       # name of the used testing variant
    self.variant_flags = []   # list of strings, flags specific to this test

    # Fields used by the test processors.
    self.origin = None # Test that this test is subtest of.
    self.processor = None # Processor that created this subtest.
    self.procid = '%s/%s' % (self.suite.name, self.name) # unique id
    self.keep_output = False # Can output of this test be dropped

    # Test config contains information needed to build the command.
    self._test_config = test_config
    self._random_seed = None # Overrides test config value if not None

    # Outcomes
    self._statusfile_outcomes = None
    self.expected_outcomes = None
    self._statusfile_flags = None

    self._prepare_outcomes()

  def create_subtest(self, processor, subtest_id, variant=None, flags=None,
                     keep_output=False, random_seed=None):
    subtest = copy.copy(self)
    subtest.origin = self
    subtest.processor = processor
    subtest.procid += '.%s' % subtest_id
    subtest.keep_output |= keep_output
    if random_seed:
      subtest._random_seed = random_seed
    if flags:
      subtest.variant_flags = subtest.variant_flags + flags
    if variant is not None:
      assert self.variant is None
      subtest.variant = variant
      subtest._prepare_outcomes()
    return subtest

  def _prepare_outcomes(self, force_update=True):
    if force_update or self._statusfile_outcomes is None:
      def is_flag(outcome):
        return outcome.startswith('--')
      def not_flag(outcome):
        return not is_flag(outcome)

      outcomes = self.suite.statusfile.get_outcomes(self.name, self.variant)
      self._statusfile_outcomes = filter(not_flag, outcomes)
      self._statusfile_flags = filter(is_flag, outcomes)
    self.expected_outcomes = (
      self._parse_status_file_outcomes(self._statusfile_outcomes))

  def _parse_status_file_outcomes(self, outcomes):
    if (statusfile.FAIL_SLOPPY in outcomes and
        '--use-strict' not in self.variant_flags):
      return outproc.OUTCOMES_FAIL

    expected_outcomes = []
    if (statusfile.FAIL in outcomes or
        statusfile.FAIL_OK in outcomes):
      expected_outcomes.append(statusfile.FAIL)
    if statusfile.CRASH in outcomes:
      expected_outcomes.append(statusfile.CRASH)

    # Do not add PASS if there is nothing else. Empty outcomes are converted to
    # the global [PASS].
    if expected_outcomes and statusfile.PASS in outcomes:
      expected_outcomes.append(statusfile.PASS)

    # Avoid creating multiple instances of a list with a single FAIL.
    if expected_outcomes == outproc.OUTCOMES_FAIL:
      return outproc.OUTCOMES_FAIL
    return expected_outcomes or outproc.OUTCOMES_PASS

  @property
  def do_skip(self):
    return statusfile.SKIP in self._statusfile_outcomes

  @property
  def is_slow(self):
    return statusfile.SLOW in self._statusfile_outcomes

  @property
  def is_fail_ok(self):
    return statusfile.FAIL_OK in self._statusfile_outcomes

  @property
  def is_pass_or_fail(self):
    return (statusfile.PASS in self._statusfile_outcomes and
            statusfile.FAIL in self._statusfile_outcomes and
            statusfile.CRASH not in self._statusfile_outcomes)

  @property
  def only_standard_variant(self):
    return statusfile.NO_VARIANTS in self._statusfile_outcomes

  def get_command(self):
    params = self._get_cmd_params()
    env = self._get_cmd_env()
    shell, shell_flags = self._get_shell_with_flags()
    timeout = self._get_timeout(params)
    return self._create_cmd(shell, shell_flags + params, env, timeout)

  def _get_cmd_params(self):
    """Gets command parameters and combines them in the following order:
      - files [empty by default]
      - random seed
      - extra flags (from command line)
      - user flags (variant/fuzzer flags)
      - statusfile flags
      - mode flags (based on chosen mode)
      - source flags (from source code) [empty by default]

    The best way to modify how parameters are created is to only override
    methods for getting partial parameters.
    """
    return (
        self._get_files_params() +
        self._get_random_seed_flags() +
        self._get_extra_flags() +
        self._get_variant_flags() +
        self._get_statusfile_flags() +
        self._get_mode_flags() +
        self._get_source_flags() +
        self._get_suite_flags()
    )

  def _get_cmd_env(self):
    return {}

  def _get_files_params(self):
    return []

  def _get_random_seed_flags(self):
    return ['--random-seed=%d' % self.random_seed]

  @property
  def random_seed(self):
    return self._random_seed or self._test_config.random_seed

  def _get_extra_flags(self):
    return self._test_config.extra_flags

  def _get_variant_flags(self):
    return self.variant_flags

  def _get_statusfile_flags(self):
    """Gets runtime flags from a status file.

    Every outcome that starts with "--" is a flag.
    """
    return self._statusfile_flags

  def _get_mode_flags(self):
    return self._test_config.mode_flags

  def _get_source_flags(self):
    return []

  def _get_suite_flags(self):
    return []

  def _get_shell_with_flags(self):
    shell = self.get_shell()
    shell_flags = []
    if shell == 'd8':
      shell_flags.append('--test')
    if utils.IsWindows():
      shell += '.exe'
    return shell, shell_flags

  def _get_timeout(self, params):
    timeout = self._test_config.timeout
    if "--stress-opt" in params:
      timeout *= 4
    if "--noenable-vfp3" in params:
      timeout *= 2

    # TODO(majeski): make it slow outcome dependent.
    timeout *= 2
    return timeout

  def get_shell(self):
    return 'd8'

  def _get_suffix(self):
    return '.js'

  def _create_cmd(self, shell, params, env, timeout):
    return command.Command(
      cmd_prefix=self._test_config.command_prefix,
      shell=os.path.abspath(os.path.join(self._test_config.shell_dir, shell)),
      args=params,
      env=env,
      timeout=timeout,
      verbose=self._test_config.verbose
    )

  def _parse_source_flags(self, source=None):
    source = source or self.get_source()
    flags = []
    for match in re.findall(FLAGS_PATTERN, source):
      flags += shlex.split(match.strip())
    return flags

  def is_source_available(self):
    return self._get_source_path() is not None

  def get_source(self):
    with open(self._get_source_path()) as f:
      return f.read()

  def _get_source_path(self):
    return None

  @property
  def output_proc(self):
    if self.expected_outcomes is outproc.OUTCOMES_PASS:
      return outproc.DEFAULT
    return outproc.OutProc(self.expected_outcomes)

  def __cmp__(self, other):
    # Make sure that test cases are sorted correctly if sorted without
    # key function. But using a key function is preferred for speed.
    return cmp(
        (self.suite.name, self.name, self.variant),
        (other.suite.name, other.name, other.variant)
    )

  def __str__(self):
    return self.suite.name + '/' + self.name
