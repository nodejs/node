# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Run the GDB bridge tests against prepared core files."""

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
from .helpers.utils import get_gdb_core_test_config
from .helpers.utils import run_debugger_command

_CONFIG = cast(DebuggerTestConfig, None)
_CORE_DIR = cast(str, None)


def setUpModule():
  global _CONFIG, _CORE_DIR
  _CONFIG = get_gdb_core_test_config()
  assert _CONFIG.core_dir is not None
  _CORE_DIR = os.path.abspath(_CONFIG.core_dir)


def run_gdb_core(config, binary_path, core_path):
  """Run one GDB backtrace command against a prepared core file."""
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
      f"core-file {os.path.abspath(core_path)}",
      "-ex",
      "bt",
      "-ex",
      "quit",
  ]
  return run_debugger_command(command, config.debug_helper_lib)


class GdbCoreBacktraceTest(unittest.TestCase):
  """Checks the d8 throw fixture backtrace loaded from a core file."""

  def test_backtrace(self):
    """Ensure the prepared backtrace core yields the expected annotations."""
    output = run_gdb_core(_CONFIG, _CONFIG.d8_binary,
                          os.path.join(_CORE_DIR, "throw.core"))
    failure = check_backtrace(output, "GDB core")
    if failure is not None:
      self.fail(failure)


class GdbCoreCorruptionTest(unittest.TestCase):
  """Checks corruption-harness backtraces loaded from core files under GDB."""

  def test_corruption_cases(self):
    """Verify each prepared corruption core yields the expected trace."""
    for case in get_corruption_cases(__file__):
      with self.subTest(corruption=case.name):
        output = run_gdb_core(_CONFIG, _CONFIG.corruption_binary,
                              os.path.join(_CORE_DIR, f"{case.name}.core"))
        failure = check_corruption(output, case, "GDB core")
        if failure is not None:
          self.fail(failure)


class GdbInspectCoreTest(unittest.TestCase):
  """Checks `v8 inspect` end-to-end with a core dump in GDB."""

  def _session(self):
    return GdbSession(
        _CONFIG,
        target_binary=_CONFIG.d8_binary,
        core_path=os.path.join(_CORE_DIR, "inspect-fixture.core"),
    )

  def test_inspect_receiver(self):
    """Checks `v8 inspect` on a receiver in a core dump."""
    with self._session() as session:
      check_inspect_receiver(session)

  def test_inspect_recurses_into_map(self):
    """Checks `v8 inspect` on a Map by following the receiver's `.map` address."""
    with self._session() as session:
      check_inspect_recurses_into_map(session)


class GdbCorruptedMapCoreTest(unittest.TestCase):
  """Checks `v8 inspect` on objects with corrupted Map from a core dump in GDB."""

  def _session(self):
    return GdbSession(
        _CONFIG,
        target_binary=_CONFIG.corruption_binary,
        core_path=os.path.join(_CORE_DIR, "corrupted-map.core"),
    )

  def test_inspect_corrupted_map_degrades(self):
    """Checks `v8 inspect` on a corrupted Map slot can be rendered without crashes."""
    with self._session() as session:
      check_corrupted_map_inspect(session)


if __name__ == "__main__":
  unittest.main()
