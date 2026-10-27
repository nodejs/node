#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runs the ASD-STE100 linter on the text of a Redline change.

It checks three kinds of text:
  - the Markdown files in experimental/redline that the change adds or edits,
  - the commit message of HEAD,
  - the comments that the change adds to the C++, Python, shell and GN files
    in experimental/redline.

A Markdown file fails if it has more hard findings than at the base revision.
The commit message and the added comments fail on any hard finding. Advisory
findings (for example passive voice) never fail, but the script shows them.

Run it from the root of the V8 checkout. Exit status: 0 if all checks pass,
1 if a check fails, 3 if the linter is not available.
"""

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile

DEFAULT_LINTER = ('/google/src/files/head/depot/google3/learning/gemini/agents/'
                  'skills/communication/asd_ste100/ste_lint.py')
SPEC = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), 'ste_lint.vpython3')
REDLINE = 'experimental/redline/'
FOOTER = re.compile(r'^[A-Za-z][A-Za-z-]*: \S')
HUNK = re.compile(r'^@@ -\d+(?:,\d+)? \+(\d+)(?:,\d+)? @@')
SKIP_COMMENT = re.compile(r'^(NOLINT|clang-format|namespace\b|IWYU|=+$|-+$)')
LICENSE = re.compile(r'Copyright .* the V8 project authors|'
                     r'Use of this source code is governed|'
                     r'found in the LICENSE file')
CPP_FILES = ('.cc', '.h')
SCRIPT_FILES = ('.py', '.sh', '.gn', '.gni')


def git(*args):
  return subprocess.run(
      ['git'] + list(args), check=True, capture_output=True, text=True).stdout


def default_base():
  for ref in ('@{upstream}', 'origin/main'):
    try:
      return git('merge-base', 'HEAD', ref).strip()
    except subprocess.CalledProcessError:
      continue
  sys.exit('Cannot find the base revision. Use --base=REV.')


def lint(linter, path):
  """Returns the hard and the advisory findings for the file at path."""
  proc = subprocess.run(
      ['vpython3', '-vpython-spec', SPEC, linter, '--json', path],
      capture_output=True,
      text=True)
  try:
    findings = json.loads(proc.stdout).get('violations', [])
  except json.JSONDecodeError:
    sys.exit(f'The linter failed on {path}:\n{proc.stdout}{proc.stderr}')
  hard = [f for f in findings if f.get('level') != 'advisory']
  advisory = [f for f in findings if f.get('level') == 'advisory']
  return hard, advisory


def lint_text(linter, text):
  with tempfile.NamedTemporaryFile('w', suffix='.md', delete=False) as f:
    f.write(text)
  try:
    return lint(linter, f.name)
  finally:
    os.unlink(f.name)


def changed_markdown(base):
  names = git('diff', '--name-only', '--diff-filter=AMR', base, '--', REDLINE)
  return [
      name for name in names.splitlines()
      if name.endswith('.md') and os.path.isfile(name)
  ]


def commit_message(base):
  if git('rev-parse', 'HEAD') == git('rev-parse', base):
    return None
  lines = git('log', '-1', '--format=%B', 'HEAD').rstrip('\n').split('\n')
  while lines and (not lines[-1].strip() or FOOTER.match(lines[-1])):
    lines.pop()
  return '\n'.join(lines) + '\n'


def cpp_comment(code):
  """Returns the text after the // of a C++ comment in a line, or None."""
  for match in re.finditer('//', code):
    before = code[:match.start()]
    quotes = before.count('"') - before.count('\\"')
    if quotes % 2 == 0:  # The // is not in a string literal.
      return code[match.end():]
  return None


def comment_of(path, code):
  """Returns (text, full_line, list_item) for the comment in a line of code.

  The text is None if the line has no comment to check. A full-line comment
  continues the paragraph of the line before it. A list item starts a new
  paragraph: an indented line, or a line that starts with "-" or "*".
  """
  stripped = code.lstrip()
  if path.endswith(CPP_FILES):
    raw = cpp_comment(code)
    full_line = stripped.startswith('//')
  elif stripped.startswith('#') and not stripped.startswith('#!'):
    raw = stripped[1:]
    full_line = True
  else:
    return None, False, False
  if raw is None:
    return None, False, False
  text = raw.strip()
  if not text or SKIP_COMMENT.match(text) or LICENSE.search(text):
    return None, False, False
  list_item = raw.startswith('  ') or text.startswith(('-', '*'))
  return text, full_line, list_item


def added_comments(base):
  """Returns (paragraph, origin) pairs for the comments that the change adds.

  The comments come from C++, Python, shell and GN files. Consecutive
  full-line comments form one paragraph, so that the linter sees whole
  sentences. A comment after code is a paragraph of its own.
  """
  patterns = [REDLINE + '*' + ext for ext in CPP_FILES + SCRIPT_FILES]
  diff = git('diff', '-U0', base, '--', *patterns)
  paragraphs = []
  path, line, can_continue = None, 0, False
  for row in diff.splitlines():
    if row.startswith('+++ '):
      path = row[6:] if row.startswith('+++ b/') else None
      can_continue = False
      continue
    hunk = HUNK.match(row)
    if hunk:
      line = int(hunk.group(1))
      can_continue = False
      continue
    if path is None or not row.startswith('+'):
      continue
    text, full_line, list_item = comment_of(path, row[1:])
    if text is None:
      can_continue = False
    elif full_line and can_continue and not list_item:
      previous, origin = paragraphs[-1]
      paragraphs[-1] = (previous + ' ' + text, origin)
    else:
      paragraphs.append((text, f'{path}:{line}'))
      can_continue = full_line
    line += 1
  return paragraphs


def show(findings, where=None):
  for f in findings:
    origin = where(f['line']) if where else f'line {f["line"]}'
    print(f'      {origin}: {f["rule"]} [{f.get("match", "")}] '
          f'{f.get("message", "")}')


def main():
  parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
  parser.add_argument(
      '--base',
      help='the base revision (default: the merge '
      'base with the upstream branch, or origin/main)')
  parser.add_argument(
      '--verbose', action='store_true', help='also show the advisory findings')
  args = parser.parse_args()
  if not os.path.isdir(REDLINE):
    sys.exit('Run this script from the root of the V8 checkout.')
  linter = os.environ.get('STE_LINT', DEFAULT_LINTER)
  if not os.path.isfile(linter):
    print(f'SKIP  the STE linter is not at {linter}. Set STE_LINT.')
    return 3
  base = args.base or default_base()
  failed = False

  for name in changed_markdown(base):
    hard, advisory = lint(linter, name)
    base_hard, base_advisory = [], []
    if subprocess.run(['git', 'cat-file', '-e', f'{base}:{name}'],
                      capture_output=True).returncode == 0:
      base_hard, base_advisory = lint_text(linter, git('show',
                                                       f'{base}:{name}'))
    ok = len(hard) <= len(base_hard)
    failed |= not ok
    print(f'{"PASS" if ok else "FAIL"}  {name}: hard {len(hard)} '
          f'(base {len(base_hard)}), advisory {len(advisory)} '
          f'(base {len(base_advisory)})')
    if not ok:
      show(hard)
    if args.verbose:
      show(advisory)

  message = commit_message(base)
  if message is None:
    print('SKIP  commit message: HEAD is the base revision. Commit first.')
  else:
    hard, advisory = lint_text(linter, message)
    failed |= bool(hard)
    print(f'{"FAIL" if hard else "PASS"}  commit message: hard {len(hard)}, '
          f'advisory {len(advisory)}')
    show(hard)
    if args.verbose:
      show(advisory)

  paragraphs = added_comments(base)
  if not paragraphs:
    print('PASS  added comments: none')
  else:
    hard, advisory = lint_text(
        linter, '\n\n'.join(text for text, _ in paragraphs) + '\n')
    origins = [origin for _, origin in paragraphs]
    where = lambda temp_line: origins[min((temp_line - 1) // 2,
                                          len(origins) - 1)]
    failed |= bool(hard)
    print(f'{"FAIL" if hard else "PASS"}  added comments ({len(paragraphs)}): '
          f'hard {len(hard)}, advisory {len(advisory)}')
    show(hard, where)
    if args.verbose:
      show(advisory, where)
  return 1 if failed else 0


if __name__ == '__main__':
  sys.exit(main())
