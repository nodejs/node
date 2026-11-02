#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Build action behind the checked-in generated headers in src/gen/.

  compare-checked-in-headers.py (--verify | --update) --stamp STAMP
      --pair GENERATED=CHECKED_IN ...

Not meant to be run by hand; BUILD.gn invokes it after the generators,
with one --pair per checked-in header, as two targets:

  verify_checked_in_generated_headers  (--verify)
      Fails if any GENERATED file differs from its CHECKED_IN copy and
      prints the regeneration command. Every build depends on it.
  regen_checked_in_generated_headers   (--update)
      Overwrites each CHECKED_IN copy that differs. Nothing depends on
      it; tools/dev/gen-checked-in-headers.py (`gm.py gen-checked-in-headers`)
      builds it to regenerate.

Both touch STAMP on success. See src/gen/README.md.
"""

import argparse
import difflib
import itertools
import pathlib
import sys

parser = argparse.ArgumentParser(allow_abbrev=False)
mode = parser.add_mutually_exclusive_group(required=True)
mode.add_argument(
    '--verify',
    action='store_true',
    help='fail if any checked-in copy differs from the generated file')
mode.add_argument(
    '--update',
    action='store_true',
    help='copy generated files over the checked-in copies')
parser.add_argument('--stamp', required=True, help='stamp file to touch')
parser.add_argument(
    '--regen-cmd',
    default='tools/dev/gm.py gen-checked-in-headers',
    help='regeneration command named in the error message')
parser.add_argument(
    '--pair', action='append', default=[], metavar='GENERATED=CHECKED_IN')
args = parser.parse_args()

pairs = [pair.split('=', 1) for pair in args.pair]


def contents(path):
  """The file contents as they should be checked in. The generators write
  in text mode, so on Windows the files have CRLF line endings, while the
  checked-in copies have LF (.gitattributes sets LF for the whole tree)."""
  return pathlib.Path(path).read_bytes().replace(b'\r\n', b'\n')


def matches(generated, checked_in):
  path = pathlib.Path(checked_in)
  return path.exists() and path.read_bytes() == contents(generated)


# Usually only a few lines differ, and the diff shows which. Cap it so
# that a completely different file does not fill the build log.
DIFF_LINES = 40


def diff(generated, checked_in):

  def lines(path):
    if not pathlib.Path(path).exists():
      return []
    return contents(path).decode('utf-8', 'replace').splitlines(keepends=True)

  delta = difflib.unified_diff(
      lines(checked_in),
      lines(generated),
      fromfile=checked_in,
      tofile=generated)
  head = list(itertools.islice(delta, DIFF_LINES))
  if next(delta, None) is not None:
    head.append(f'[... truncated after {DIFF_LINES} lines]\n')
  return ''.join(head)


if args.update:
  for generated, checked_in in pairs:
    if not matches(generated, checked_in):
      pathlib.Path(checked_in).write_bytes(contents(generated))
      print(f'Updated {checked_in}')
else:
  stale = [(g, c) for g, c in pairs if not matches(g, c)]
  if stale:
    for generated, checked_in in stale:
      print(
          f'Checked-in generated file is out of date: {checked_in}',
          file=sys.stderr)
      print(diff(generated, checked_in), end='', file=sys.stderr)
    print(
        f'Run `{args.regen_cmd}` and commit the result. This also happens '
        'when main has changed the generated headers since you last '
        'regenerated, e.g. on the CQ: rebase first.',
        file=sys.stderr)
    print(
        'If this build sets GN args that change the generated headers, add '
        'them to the v8_use_checked_in_generated_headers condition in '
        'BUILD.gn instead: see src/gen/README.md.',
        file=sys.stderr)
    sys.exit(1)

pathlib.Path(args.stamp).touch()
