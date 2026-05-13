# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Run the LLDB bridge tests against prepared core files."""

import os
import unittest

from .helpers.backtrace import check_backtrace
from .helpers.corruptions import check_corruption
from .helpers.corruptions import get_corruption_cases
from .helpers.utils import get_lldb_core_test_config
from .helpers.utils import run_debugger_command


def run_lldb_core(config, binary_path, core_path):
  """Run one LLDB backtrace command against a prepared core file."""
  command = [
      config.debugger_binary,
      "-b",
      "-O",
      f'command script import "{os.path.abspath(config.plugin_path)}"',
      "-O",
      f'target create --core "{os.path.abspath(core_path)}" '
      f'"{os.path.abspath(binary_path)}"',
      "-O",
      "bt",
      "-O",
      "quit",
  ]
  return run_debugger_command(command, config.debug_helper_lib)


class LldbCoreBacktraceTest(unittest.TestCase):
  """Checks the d8 throw fixture backtrace loaded from a core file."""

  @classmethod
  def setUpClass(cls):
    cls.config = get_lldb_core_test_config()
    assert cls.config.core_dir is not None
    cls.core_dir = os.path.abspath(cls.config.core_dir)

  def test_backtrace(self):
    """Ensure the prepared backtrace core yields the expected annotations."""
    output = run_lldb_core(self.config, self.config.d8_binary,
                           os.path.join(self.core_dir, "throw.core"))
    failure = check_backtrace(output, "LLDB core")
    if failure is not None:
      self.fail(failure)


class LldbCoreCorruptionTest(unittest.TestCase):
  """Checks corruption-harness backtraces loaded from core files under LLDB."""

  @classmethod
  def setUpClass(cls):
    cls.config = get_lldb_core_test_config()
    assert cls.config.core_dir is not None
    cls.core_dir = os.path.abspath(cls.config.core_dir)

  def test_corruption_cases(self):
    """Verify each prepared corruption core yields the expected trace."""
    for case in get_corruption_cases(__file__):
      with self.subTest(corruption=case.name):
        output = run_lldb_core(self.config, self.config.corruption_binary,
                               os.path.join(self.core_dir, f"{case.name}.core"))
        failure = check_corruption(output, case, "LLDB core")
        if failure is not None:
          self.fail(failure)


if __name__ == "__main__":
  unittest.main()
