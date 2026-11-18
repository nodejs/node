#!/usr/bin/env vpython3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Strips the leading [P0]-[P4] priority prefix from the local commit and Gerrit CL."""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys

PRIORITY_PREFIX_RE = re.compile(r"^\[P([0-4])\]\s*(.+)$")


def strip_priority_from_message(message: str) -> tuple[bool, str, str, str]:
  """Strips a leading [P0]-[P4] prefix from the first title line of message.

  Returns (stripped, new_message, old_title, new_title).
  """
  lines = message.splitlines(keepends=True)
  for i, line in enumerate(lines):
    stripped_line = line.strip()
    if not stripped_line or stripped_line.startswith("#"):
      continue
    ending = line[len(line.rstrip("\r\n")):]
    m = PRIORITY_PREFIX_RE.match(stripped_line)
    if not m:
      return False, message, stripped_line, stripped_line
    new_title = m.group(2)
    lines[i] = new_title + ending
    return True, "".join(lines), stripped_line, new_title
  return False, message, "", ""


def _git_env() -> dict[str, str]:
  env = os.environ.copy()
  env["EDITOR"] = "true"
  env["GIT_EDITOR"] = "true"
  env["SKIP_GCE_AUTH_FOR_GIT"] = "1"
  return env


def get_local_commit_message(cwd: str | None = None) -> str:
  res = subprocess.run(
      ["git", "--no-pager", "log", "-1", "--format=%B"],
      cwd=cwd,
      env=_git_env(),
      capture_output=True,
      text=True,
      check=False,
  )
  if res.returncode != 0:
    raise RuntimeError(
        f"Failed to read local git commit message: {res.stderr.strip()}")
  return res.stdout


def has_gerrit_issue(cwd: str | None = None) -> bool:
  res = subprocess.run(
      ["git", "cl", "issue"],
      cwd=cwd,
      env=_git_env(),
      capture_output=True,
      text=True,
      check=False,
  )
  if res.returncode != 0:
    return False
  out = res.stdout.strip()
  return bool(out and "None" not in out)


def get_gerrit_description(cwd: str | None = None) -> str:
  res = subprocess.run(
      ["git", "cl", "description", "-d"],
      cwd=cwd,
      env=_git_env(),
      capture_output=True,
      text=True,
      check=False,
  )
  if res.returncode != 0 or not res.stdout.strip():
    detail = res.stderr.strip() or "empty description returned"
    raise RuntimeError(f"Failed to read Gerrit CL description: {detail}")
  return res.stdout


def amend_local_commit(message: str, cwd: str | None = None) -> None:
  res = subprocess.run(
      ["git", "commit", "--amend", "-F", "-"],
      cwd=cwd,
      env=_git_env(),
      input=message,
      capture_output=True,
      text=True,
      check=False,
  )
  if res.returncode != 0:
    raise RuntimeError(
        f"Failed to amend local commit message: {res.stderr.strip()}")


def update_gerrit_description(message: str, cwd: str | None = None) -> None:
  res = subprocess.run(
      ["git", "cl", "description", "-n", "-"],
      cwd=cwd,
      env=_git_env(),
      input=message,
      capture_output=True,
      text=True,
      check=False,
  )
  if res.returncode != 0:
    raise RuntimeError(
        f"Failed to update Gerrit CL description: {res.stderr.strip()}")


def main(argv: list[str] | None = None) -> int:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument(
      "--local-only",
      action="store_true",
      help="Only strip the prefix from the local HEAD commit without calling git cl.",
  )
  parser.add_argument(
      "--cwd",
      default=None,
      help="Repository working directory (defaults to current directory).",
  )
  args = parser.parse_args(argv)

  try:
    sync_gerrit = not args.local_only and has_gerrit_issue(cwd=args.cwd)
    if sync_gerrit:
      base_message = get_gerrit_description(cwd=args.cwd)
    else:
      base_message = get_local_commit_message(cwd=args.cwd)

    stripped, new_message, old_title, new_title = strip_priority_from_message(
        base_message)
    if not stripped:
      print(f'No [P0]-[P4] priority prefix to strip in: "{old_title}"')
      return 0

    amend_local_commit(new_message, cwd=args.cwd)
    if sync_gerrit:
      update_gerrit_description(new_message, cwd=args.cwd)
      print(
          f'Stripped priority prefix locally and on Gerrit: "{old_title}" -> "{new_title}"'
      )
    else:
      print(f'Stripped priority prefix locally: "{old_title}" -> "{new_title}"')
    return 0
  except RuntimeError as e:
    print(f"Error: {e}", file=sys.stderr)
    return 1


if __name__ == "__main__":
  sys.exit(main())
