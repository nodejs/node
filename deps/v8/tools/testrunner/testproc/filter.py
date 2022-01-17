# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import defaultdict
import fnmatch
import os

from . import base


class StatusFileFilterProc(base.TestProcFilter):
  """Filters tests by outcomes from status file.

  Status file has to be loaded before using this function.

  Args:
    slow_tests_mode: What to do with slow tests.
    pass_fail_tests_mode: What to do with pass or fail tests.

  Mode options:
    None (default): don't skip
    "skip": skip if slow/pass_fail
    "run": skip if not slow/pass_fail
  """

  def __init__(self, slow_tests_mode, pass_fail_tests_mode):
    super(StatusFileFilterProc, self).__init__()
    self._slow_tests_mode = slow_tests_mode
    self._pass_fail_tests_mode = pass_fail_tests_mode

  def _filter(self, test):
    return (
      test.do_skip or
      self._skip_slow(test.is_slow) or
      self._skip_pass_fail(test.is_pass_or_fail)
    )

  def _skip_slow(self, is_slow):
    return (
      (self._slow_tests_mode == 'run' and not is_slow) or
      (self._slow_tests_mode == 'skip' and is_slow)
    )

  def _skip_pass_fail(self, is_pass_fail):
    return (
      (self._pass_fail_tests_mode == 'run' and not is_pass_fail) or
      (self._pass_fail_tests_mode == 'skip' and is_pass_fail)
    )


class NameFilterProc(base.TestProcFilter):
  """Filters tests based on command-line arguments.

  args can be a glob: asterisks in any position of the name
  represent zero or more characters. Without asterisks, only exact matches
  will be used with the exeption of the test-suite name as argument.
  """
  def __init__(self, args):
    super(NameFilterProc, self).__init__()

    self._globs = defaultdict(list)
    self._exact_matches = defaultdict(dict)
    for a in args:
      argpath = a.split('/')
      suitename = argpath[0]
      path = '/'.join(argpath[1:]) or '*'
      if '*' in path:
        self._globs[suitename].append(path)
      else:
        self._exact_matches[suitename][path] = True

    for s, globs in list(self._globs.items()):
      if not globs or '*' in globs:
        self._globs[s] = ['*']

  def _filter(self, test):
    globs = self._globs.get(test.suite.name, [])
    for g in globs:
      if g == '*': return False
      if fnmatch.fnmatch(test.path, g):
        return False
    exact_matches = self._exact_matches.get(test.suite.name, {})
    if test.path in exact_matches: return False
    if os.sep != '/':
      unix_path = test.path.replace(os.sep, '/')
      if unix_path in exact_matches: return False
    # Filter out everything else.
    return True
