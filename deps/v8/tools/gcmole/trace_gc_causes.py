#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import argparse
from pathlib import Path


def parse_gccauses(file_path):
  """Parses the gccauses flat file into a caller -> list(callees) map."""
  causes = {}
  with open(file_path, "r") as f:
    lines = f.readlines()

  idx = 0
  num_lines = len(lines)
  while idx < num_lines:
    line = lines[idx].strip()
    if not line or line == "start,nested" or line == "end,nested":
      idx += 1
      continue

    # Line format: mangled_name,demangled_name
    parts = line.split(",", 1)
    mangled = parts[0]
    demangled = parts[1] if len(parts) > 1 else mangled

    # Check if definition block follows
    if idx + 1 < num_lines and lines[idx + 1].strip() == "start,nested":
      idx += 2
      callees = []
      while idx < num_lines and lines[idx].strip() != "end,nested":
        callee_line = lines[idx].strip()
        if callee_line:
          callee_parts = callee_line.split(",", 1)
          callees.append(
              (callee_parts[0],
               callee_parts[1] if len(callee_parts) > 1 else callee_parts[0]))
        idx += 1
      causes[mangled] = {"demangled": demangled, "callees": callees}
    else:
      # Leaf node or declaration only
      if mangled not in causes:
        causes[mangled] = {"demangled": demangled, "callees": []}
    idx += 1
  return causes


import re


def print_tree(causes,
               mangled,
               visited=None,
               indent="",
               depth=0,
               max_depth=10,
               is_caller_virtual=False):
  if visited is None:
    visited = set()

  node = causes.get(mangled)
  if not node:
    # Special primitives
    if mangled == "<GC>":
      print(f"{indent}└── <GC> [Heap Allocation / Collect Garbage]")
    elif mangled == "<Safepoint>":
      print(f"{indent}└── <Safepoint>")
    else:
      print(f"{indent}└── {mangled} (unknown)")
    return

  demangled = node["demangled"]
  # Strip namespaces for readability in tree representation
  short_name = demangled.replace("v8::internal::", "")
  # Strip ugly anonymous namespace TU suffixes (like $_usr_local_...)
  short_name = re.sub(r'\$_[a-zA-Z0-9_]{9,}', '', short_name)

  # Construct semantic prefixes
  prefix = ""
  is_virtual = mangled.startswith("VIRTUAL-") or mangled.startswith(
      "VIRTUALTHIS-")
  if is_virtual:
    prefix = "(virtual) "
  elif mangled.startswith("SYNTHOVER-") or is_caller_virtual:
    prefix = "(override) "

  display_name = f"{prefix}{short_name}"

  if mangled in visited:
    print(f"{indent}└── {display_name} [Recursion Cycle]")
    return

  print(f"{indent}└── {display_name}")

  if not node["callees"] or depth >= max_depth:
    return

  visited.add(mangled)
  new_indent = indent + "    "
  for callee_mangled, _ in node["callees"]:
    print_tree(
        causes,
        callee_mangled,
        visited.copy(),
        new_indent,
        depth + 1,
        max_depth,
        is_caller_virtual=is_virtual)


def main():
  parser = argparse.ArgumentParser(
      description="Trace and print GCMole GC cause call trees.")
  parser.add_argument(
      "function", help="Function name or mangled signature to trace from.")
  parser.add_argument("--gccauses", type=Path, help="Path to gccauses file.")
  parser.add_argument(
      "--max-depth", type=int, default=16, help="Maximum recursion depth.")
  args = parser.parse_args()

  # Auto-locate gccauses if not provided
  gccauses_path = args.gccauses
  if not gccauses_path:
    for p in [
        Path("out/x64.release/gen/tools/gcmole/gccauses"),
        Path("gccauses")
    ]:
      if p.exists():
        gccauses_path = p
        break

  if not gccauses_path or not gccauses_path.exists():
    print(
        "Error: Could not locate 'gccauses' database file. Please run GCMole full scan first or specify via --gccauses.",
        file=sys.stderr)
    sys.exit(1)

  causes = parse_gccauses(gccauses_path)

  # Find matching mangled keys
  matched_keys = []
  for key, val in causes.items():
    if args.function in key or args.function in val["demangled"]:
      matched_keys.append(key)

  if not matched_keys:
    print(
        f"Error: Could not find any function matching '{args.function}' in gccauses database.",
        file=sys.stderr)
    sys.exit(1)

  if len(matched_keys) > 1:
    print(f"Found multiple matching functions for '{args.function}':")
    for idx, key in enumerate(matched_keys):
      print(f"  [{idx}] {causes[key]['demangled']}")
    print("Tracing from the first match...")

  print(f"\nTracing GC Call Tree from: {causes[matched_keys[0]]['demangled']}")
  print("=" * 80)
  print_tree(causes, matched_keys[0], max_depth=args.max_depth)
  print("=" * 80 + "\n")


if __name__ == "__main__":
  main()
