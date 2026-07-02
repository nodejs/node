# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Run the GDB bridge tests and assert expected output."""

import os
import unittest
from typing import cast

from .helpers.backtrace import check_backtrace
from .helpers.corruptions import check_corruption
from .helpers.corruptions import check_corrupted_map_inspect
from .helpers.corruptions import get_corruption_cases
from .helpers.inspect import check_inspect_receiver
from .helpers.inspect import check_inspect_recurses_into_map
from .helpers.session import GdbSession
from .helpers.utils import DebuggerTestConfig
from .helpers.utils import FIXTURES_DIR
from .helpers.utils import get_gdb_live_test_config
from .helpers.utils import run_debugger_command

_CONFIG = cast(DebuggerTestConfig, None)


def setUpModule():
  global _CONFIG
  _CONFIG = get_gdb_live_test_config()


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

  def test_backtrace(self):
    """Ensure the regular nested JS backtrace is annotated as expected."""
    script = os.path.join(FIXTURES_DIR, "throw.js")
    output = run_gdb_live(
        _CONFIG, _CONFIG.d8_binary,
        f'{_CONFIG.d8_args} --abort-on-uncaught-exception "{script}"')
    failure = check_backtrace(output, "GDB")
    if failure is not None:
      self.fail(failure)


class GdbCorruptionTest(unittest.TestCase):
  """Checks corruption-harness backtraces under GDB."""

  def test_corruption_cases(self):
    """Verify each corruption case yields the expected trace."""
    for case in get_corruption_cases(__file__):
      with self.subTest(corruption=case.name):
        output = run_gdb_live(_CONFIG, _CONFIG.corruption_binary,
                              f'"{case.script_path}"')
        failure = check_corruption(output, case, "GDB")
        if failure is not None:
          self.fail(failure)


class GdbInspectLiveTest(unittest.TestCase):
  """Checks `v8 inspect` end-to-end with a live d8 process in GDB."""

  def _session(self):
    script = os.path.join(FIXTURES_DIR, "inspect-fixture.js")
    return GdbSession(
        _CONFIG,
        target_binary=_CONFIG.d8_binary,
        target_args=f'{_CONFIG.d8_args} --abort-on-uncaught-exception '
        f'"{script}"',
    )

  def test_inspect_receiver(self):
    """Checks `v8 inspect` on a receiver in a live session."""
    with self._session() as session:
      session.run_to_abort()
      check_inspect_receiver(session)

  def test_inspect_recurses_into_map(self):
    """Checks `v8 inspect` on a Map by following the receiver's `.map` address in a live session."""
    with self._session() as session:
      session.run_to_abort()
      check_inspect_recurses_into_map(session)


class GdbCorruptedMapTest(unittest.TestCase):
  """Checks `v8 inspect` on objects with corrupted Map with a live d8 process in GDB."""

  def _session(self):
    script = os.path.join(FIXTURES_DIR, "corrupted-map.js")
    return GdbSession(
        _CONFIG,
        target_binary=_CONFIG.corruption_binary,
        target_args=f'"{script}"',
    )

  def test_inspect_corrupted_map_degrades(self):
    """Checks `v8 inspect` on a corrupted Map slot can be rendered without crashes."""
    with self._session() as session:
      session.run_to_abort()
      check_corrupted_map_inspect(session)


if __name__ == "__main__":
  unittest.main()
