# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Run the GDB bridge tests and assert expected output."""

import os
import unittest

from .helpers.backtrace import check_backtrace
from .helpers.corruptions import check_corruption
from .helpers.corruptions import get_corruption_cases
from .helpers.utils import get_gdb_live_test_config
from .helpers.utils import run_debugger_command


def run_gdb_live(config, binary_path, run_arguments):
  """Run one GDB backtrace command and return its combined text output."""
  command = [
      config.debugger_binary,
      "-nx",
      "-q",
      "-batch",
      "-iex",
      "set debuginfod enabled off",
      "-iex",
      f"source {os.path.abspath(config.gdbinit_path)}",
      "-iex",
      f"source {os.path.abspath(config.plugin_path)}",
      "-ex",
      f"file {os.path.abspath(binary_path)}",
      "-ex",
      f"run {run_arguments}",
      "-ex",
      "bt",
      "-ex",
      "quit",
  ]
  return run_debugger_command(command, config.debug_helper_lib)


class GdbBacktraceTest(unittest.TestCase):
  """Checks the normal d8 throw fixture backtrace under GDB."""

  @classmethod
  def setUpClass(cls):
    cls.config = get_gdb_live_test_config()
    cls.backtrace_script = os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "fixtures", "throw.js")

  def test_backtrace(self):
    """Ensure the regular nested JS backtrace is annotated as expected."""
    output = run_gdb_live(
        self.config, self.config.d8_binary, '--abort-on-uncaught-exception '
        f'"{self.backtrace_script}"')
    failure = check_backtrace(output, "GDB")
    if failure is not None:
      self.fail(failure)


class GdbCorruptionTest(unittest.TestCase):
  """Checks corruption-harness backtraces under GDB."""

  @classmethod
  def setUpClass(cls):
    cls.config = get_gdb_live_test_config()

  def test_corruption_cases(self):
    """Verify each corruption case yields the expected trace."""
    for case in get_corruption_cases(__file__):
      with self.subTest(corruption=case.name):
        output = run_gdb_live(self.config, self.config.corruption_binary,
                              f'"{case.script_path}"')
        failure = check_corruption(output, case, "GDB")
        if failure is not None:
          self.fail(failure)


if __name__ == "__main__":
  unittest.main()
