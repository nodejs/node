# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helpers for corruption-harness test cases."""

from dataclasses import dataclass
import os
import re

from .backtrace import check_backtrace

# JS method in corrupted-map.js whose receiver gets its Map corrupted.
_CORRUPTED_MAP_FRAME = "corrupt"
_HEAPOBJECT_HEAD_RE = re.compile(r"^0x[0-9a-f]+ <HeapObject\b", re.MULTILINE)


def check_corrupted_map_inspect(session):
  """Inspect an object whose Map has an invalid instance type, which should
  render as a bare `<HeapObject>` instead of crashing.
  """
  output = session.v8_inspect(session.frame_receiver(_CORRUPTED_MAP_FRAME))
  if not _HEAPOBJECT_HEAD_RE.search(output):
    raise AssertionError(
        "expected the corrupted-Map object to render as `<HeapObject>` "
        f"but got:\n{output}")


@dataclass(frozen=True)
class CorruptionCase:
  name: str
  script_path: str
  expected_annotations: tuple[str, ...]
  absent_output: tuple[str, ...] = ()

  def resolve(self, test_dir):
    """Return one case with its fixture path resolved against the test dir."""
    return CorruptionCase(
        name=self.name,
        script_path=os.path.join(test_dir, self.script_path),
        expected_annotations=self.expected_annotations,
        absent_output=self.absent_output,
    )


CORRUPTION_CASES = (
    CorruptionCase(
        name="invalid-script-source",
        script_path="fixtures/invalid-script-source.js",
        expected_annotations=(
            "[test_func_3 @ <base>/invalid-script-source.js]"
            " (this=<addr>, argc=0)",
            "[test_func_2 @ <base>/invalid-script-source.js]"
            " (this=<addr>, argc=0)",
            "[test_func_1 @ <base>/invalid-script-source.js]"
            " (this=<addr>, argc=0)",
            "[<anonymous> @ <base>/invalid-script-source.js:1:1]"
            " (this=<addr>, argc=1)",
        ),
    ),
    CorruptionCase(
        name="invalid-shared-function-info",
        script_path="fixtures/invalid-shared-function-info.js",
        expected_annotations=(
            "[test_func_2 @ <base>/invalid-shared-function-info.js:9:21]"
            " (this=<addr>, argc=0)",
            "[test_func_1 @ <base>/invalid-shared-function-info.js:13:21]"
            " (this=<addr>, argc=0)",
            "[<anonymous> @ <base>/invalid-shared-function-info.js:1:1]"
            " (this=<addr>, argc=1)",
        ),
        absent_output=("test_func_3",),
    ),
)


def check_corruption(output, case, debugger_name):
  """Check one corruption case against its trace expectations."""
  failures = []

  failure = check_backtrace(output, f"{debugger_name} corruption",
                            case.expected_annotations)
  if failure is not None:
    failures.append(failure)

  unexpected = [entry for entry in case.absent_output if entry in output]
  if unexpected:
    lines = [output, f"\nUnexpected {debugger_name} corruption output:\n"]
    for entry in unexpected:
      lines.append(f"  {entry}\n")
    failures.append("".join(lines))

  if not failures:
    return None
  return "\n".join(failures)


def get_corruption_cases(test_file):
  """Return the resolved corruption cases for one test module."""
  test_dir = os.path.dirname(os.path.abspath(test_file))
  return tuple(case.resolve(test_dir) for case in CORRUPTION_CASES)
