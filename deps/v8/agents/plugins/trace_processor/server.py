#!/usr/bin/env vpython3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""TraceProcessor MCP server."""

import contextlib
import pathlib
import sys

# Add third_party/perfetto/python to sys.path
SRC_DIR = pathlib.Path(__file__).resolve().parents[3]
PERFETTO_PY_DIR = SRC_DIR / 'third_party/perfetto/python'
if not PERFETTO_PY_DIR.exists():
  print(
      f"Error: Perfetto Python module not found at {PERFETTO_PY_DIR}",
      file=sys.stderr)
  sys.exit(1)

if str(PERFETTO_PY_DIR) not in sys.path:
  sys.path.insert(0, str(PERFETTO_PY_DIR))

from perfetto.trace_processor import TraceProcessor
from mcp.server import fastmcp
from mcp.server.fastmcp.exceptions import ToolError

# TODO: replace this with the native perfetto skill and helpers once they
# are more mature.
mcp = fastmcp.FastMCP('trace-processor')

_tp_instance: TraceProcessor | None = None
_current_trace_path: pathlib.Path | None = None


def _assert_active_session() -> TraceProcessor:
  if not _tp_instance:
    raise ToolError(
        "No active TraceProcessor session. Please call 'launch' first "
        "with a valid trace path.")
  return _tp_instance


@mcp.tool()
def launch(trace_path: str) -> dict:
  """Launch or reuse a long-running Perfetto TraceProcessor session on an
  existing trace file. Must be called first before running any queries.

  Args:
    trace_path: Path to the Perfetto trace file to analyze.
  """
  global _tp_instance, _current_trace_path

  abs_path = pathlib.Path(trace_path).expanduser().resolve()
  if not abs_path.exists():
    raise ToolError(f"Trace file does not exist at {abs_path}")

  if _tp_instance:
    if _current_trace_path == abs_path:
      return {
          "status": "reused",
          "trace_path": str(abs_path),
          "message": f"Reusing existing active session for {abs_path}",
      }
    else:
      with contextlib.suppress(Exception):
        _tp_instance.close()
      _tp_instance = None
      _current_trace_path = None

  try:
    _tp_instance = TraceProcessor(trace=str(abs_path))
    _current_trace_path = abs_path
    return {
        "status": "launched",
        "trace_path": str(abs_path),
        "message": f"Successfully launched new session for {abs_path}",
    }
  except Exception as e:
    raise ToolError(f"Failed to launch TraceProcessor session: {str(e)}") from e


@mcp.tool()
def query(sql: str, limit: int = 100) -> dict:
  """Execute a PerfettoSQL query against the active trace session. Returns a
  list of result rows up to the specified limit.

  Args:
    sql: PerfettoSQL query string to execute against the active session.
    limit: Maximum number of rows to return (default: 100).
  """
  try:
    tp = _assert_active_session()
    res = tp.query(sql.strip())
    df = res.as_pandas_dataframe()

    total_rows = len(df)
    truncated = total_rows > limit
    df = df.head(limit)
    rows = []
    if not df.empty:
      rows = df.to_dict(orient="records")
    return {
        "rows": rows,
        "truncated": truncated,
        "count": total_rows,
        "limit": limit,
    }
  except ToolError:
    raise
  except Exception as e:
    raise ToolError(f"Error executing query: {str(e)}") from e


if __name__ == '__main__':
  mcp.run()
