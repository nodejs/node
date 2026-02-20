# Copyright 2019 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import shlex
import sys

from pathlib import Path

from testrunner.local import testsuite
from testrunner.objects import testcase

PY_FLAGS_PATTERN = re.compile(r"#\s+Flags:(.*)")


class PYTestCase(testcase.TestCase):

  def get_shell(self):
    return Path(sys.executable).stem

  def get_command(self):
    return super(PYTestCase, self).get_command()

  def _get_cmd_params(self):
    return (
        self._get_files_params() +
        ['--', self.test_config.shell_dir / 'd8'] +
        self._get_source_flags()
    )

  def _get_shell_flags(self):
    return []


class TestCase(PYTestCase):

  def __init__(self, *args, **kwargs):
    super(TestCase, self).__init__(*args, **kwargs)

    source = self.get_source()
    self._source_files = self._parse_source_files(source)
    self._source_flags = self._parse_source_flags(source)

  def _parse_source_files(self, source):
    files = []
    files.append(self._get_source_path())
    return files

  def _parse_source_flags(self, source=None):
    source = source or self.get_source()
    flags = []
    for match in re.findall(PY_FLAGS_PATTERN, source):
      flags += shlex.split(match.strip())
    return flags

  def _expected_fail(self):
    return 'fail' in self.path.parts

  def _get_files_params(self):
    return self._source_files

  def _get_source_flags(self):
    return self._source_flags

  def _get_source_path(self):
    js_file = self.suite.root / self.path_js
    if js_file.exists():
      return js_file
    return self.suite.root / self.path_and_suffix('.py')

  def skip_predictable(self):
    return super(TestCase, self).skip_predictable() or self._expected_fail()


class PYTestLoader(testsuite.GenericTestLoader):

  @property
  def excluded_files(self):
    return {'gdb_rsp.py', 'testcfg.py', '__init__.py', 'test_basic.py',
            'test_float.py', 'test_memory.py', 'test_trap.py'}

  @property
  def extensions(self):
    return ['.py']


class TestSuite(testsuite.TestSuite):

  def _test_loader_class(self):
    return PYTestLoader

  def _test_class(self):
    return TestCase
