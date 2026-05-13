# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Run the LLDB bridge tests and assert expected output."""

import os
import unittest

from .helpers.backtrace import check_backtrace
from .helpers.corruptions import check_corruption
from .helpers.corruptions import get_corruption_cases
from .helpers.utils import get_lldb_live_test_config
from .helpers.utils import run_debugger_command


def run_lldb_live(config, binary_path, run_arguments):
  """Run one LLDB backtrace command and return its combined text output."""
  command = [
      config.debugger_binary,
      "-b",
      "-O",
      f'command script import "{os.path.abspath(config.plugin_path)}"',
      "-O",
      f'target create "{os.path.abspath(binary_path)}"',
      "-O",
      f'settings set -- target.run-args {run_arguments}',
      "-O",
      "run",
      "-k",
      "bt",
      "-k",
      "quit",
  ]
  return run_debugger_command(command, config.debug_helper_lib)


class LldbBacktraceTest(unittest.TestCase):
  """Checks the normal d8 throw.js backtrace under LLDB."""

  @classmethod
  def setUpClass(cls):
    cls.config = get_lldb_live_test_config()
    cls.backtrace_script = os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "fixtures", "throw.js")

  def test_backtrace(self):
    """Ensure the regular nested JS backtrace is annotated as expected."""
    output = run_lldb_live(
        self.config, self.config.d8_binary, '--abort-on-uncaught-exception '
        f'"{self.backtrace_script}"')
    failure = check_backtrace(output, "LLDB")
    if failure is not None:
      self.fail(failure)


class LldbCorruptionTest(unittest.TestCase):
  """Checks corruption-harness backtraces under LLDB."""

  @classmethod
  def setUpClass(cls):
    cls.config = get_lldb_live_test_config()

  def test_corruption_cases(self):
    """Verify each corruption case yields the expected trace."""
    for case in get_corruption_cases(__file__):
      with self.subTest(corruption=case.name):
        output = run_lldb_live(self.config, self.config.corruption_binary,
                               f'"{case.script_path}"')
        failure = check_corruption(output, case, "LLDB")
        if failure is not None:
          self.fail(failure)


if __name__ == "__main__":
  unittest.main()
