#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Validates a commit description against Gerrit line length and Change-Id rules."""

import re
import subprocess
import sys


MAX_TITLE_LENGTH = 64
MAX_LINE_LENGTH = 72


def validate_description(desc: str) -> bool:
  if not desc or not desc.strip():
    return True

  lines = desc.splitlines()
  bad_title = None
  bad_lines = []
  seen_title = False

  footer_regex = re.compile(
      r'^(Change-Id|Reviewed-on|Reviewed-by|Commit-Queue|Cr-Commit-Position|Bug|Fixed|TAG|Conversation-ID):'
  )

  for i, line in enumerate(lines, start=1):
    # Ignore comment lines
    if line.lstrip().startswith('#'):
      continue

    if not seen_title:
      if not line.strip():
        continue
      seen_title = True
      if len(line) > MAX_TITLE_LENGTH:
        bad_title = line
      continue

    if len(line) > MAX_LINE_LENGTH:
      # Exclude URLs and short links
      if '://' in line or 'go/' in line or 'goto.google.com/' in line:
        continue
      # Exclude standard metadata footers
      if footer_regex.match(line):
        continue
      # Exclude single unbroken tokens (file paths, symbol names) without whitespace
      if ' ' not in line.strip():
        continue

      bad_lines.append(f'Line {i} ({len(line)} chars): {line[:40]}...')

  valid = True
  if bad_title:
    print(
        f'Error: The commit title is {len(bad_title)} characters long, '
        f'exceeding the {MAX_TITLE_LENGTH}-character maximum length limit '
        '(aim for ~50 characters):',
        file=sys.stderr)
    print(f'  {bad_title}', file=sys.stderr)
    print(
        f'Please shorten the commit title to at most {MAX_TITLE_LENGTH} '
        'characters (including the [component] prefix) before proceeding.',
        file=sys.stderr)
    valid = False

  if bad_lines:
    print(
        'Error: The commit description contains lines exceeding the '
        f'{MAX_LINE_LENGTH} character maximum length limit:',
        file=sys.stderr)
    for bad in bad_lines:
      print(f'  {bad}', file=sys.stderr)
    print(
        'Please wrap commit message descriptions strictly at '
        f'{MAX_LINE_LENGTH} characters before proceeding.',
        file=sys.stderr)
    valid = False

  if not valid:
    return False

  # Check Change-Id count
  change_ids = [line for line in lines if line.startswith('Change-Id:')]
  if len(change_ids) > 1:
    print(
        f'Error: Multiple Change-Id lines detected in commit message ({len(change_ids)} found).',
        file=sys.stderr)
    print(
        'Please keep only the correct Change-Id for this branch before proceeding.',
        file=sys.stderr)
    return False

  # Check Change-Id match against expected commit/CL
  try:
    expected_ids = []
    try:
      cl_proc = subprocess.run(['git', 'cl', 'desc', '-d'],
                               capture_output=True,
                               text=True,
                               check=True)
      expected_ids = [
          line.strip()
          for line in cl_proc.stdout.splitlines()
          if re.match(r'^Change-Id: I[0-9a-fA-F]+', line)
      ]
    except (subprocess.CalledProcessError, FileNotFoundError):
      pass

    if not expected_ids:
      log_proc = subprocess.run(
          ['git', '--no-pager', 'log', '-1', '--pretty=%B'],
          capture_output=True,
          text=True,
          check=True)
      expected_ids = [
          line.strip()
          for line in log_proc.stdout.splitlines()
          if re.match(r'^Change-Id: I[0-9a-fA-F]+', line)
      ]

    if expected_ids and len(change_ids) == 1:
      expected_id = expected_ids[-1]
      msg_id = change_ids[0].strip()
      if msg_id != expected_id:
        print(
            f'Error: The Change-Id in the description ({msg_id}) does not match the expected Change-Id ({expected_id}) for this branch.',
            file=sys.stderr)
        print(
            'Please preserve the original Change-Id to avoid creating duplicate CLs.',
            file=sys.stderr)
        return False
  except (subprocess.CalledProcessError, FileNotFoundError):
    pass

  return True


def main():
  if len(sys.argv) > 1 and sys.argv[1] != '-':
    desc = sys.argv[1]
  else:
    desc = sys.stdin.read()

  if not validate_description(desc):
    sys.exit(1)
  sys.exit(0)


if __name__ == '__main__':
  main()
