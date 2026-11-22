#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Compares two Redline fuzzing sessions at the same elapsed time.

Usage: ab_compare.py BASE_EVENTS AFTER_EVENTS [--every=MINUTES]

BASE_EVENTS and AFTER_EVENTS are the events.jsonl files of the two sessions.
The script reads their "stats" events, prints the coverage and the number of
executions of both sessions minute by minute, and then a summary.
CONTRIBUTE.md section 10.2 tells how to read the result.
"""

import argparse
import json
import sys

NOISE_PP = 0.5  # Two identical sessions differ by 0.25 to 0.35 points.


def load(path):
  """Returns the stats events of a session, by elapsed minute."""
  stats = {}
  with open(path, errors='replace') as f:
    for line in f:
      try:
        event = json.loads(line)
      except json.JSONDecodeError:
        continue  # An interrupted session can end with an incomplete line.
      if event.get('event') == 'stats':
        stats[round(event['t'] / 60)] = event
  return stats


def pp(fraction):
  return 100 * fraction


def main():
  parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
  parser.add_argument('base_events')
  parser.add_argument('after_events')
  parser.add_argument(
      '--every',
      type=int,
      default=5,
      help='print one row every this many minutes (default 5)')
  args = parser.parse_args()
  base, after = load(args.base_events), load(args.after_events)
  minutes = sorted(set(base) & set(after))
  if not minutes:
    sys.exit('The two sessions have no stats event at the same minute.')

  print(' min   base cov  after cov   diff (pp)   base execs  after execs')
  for minute in minutes:
    if minute % args.every and minute != minutes[-1]:
      continue
    a, b = base[minute], after[minute]
    print(
        f'{minute:4d}   {pp(a["coverage"]):7.3f}%   {pp(b["coverage"]):7.3f}%'
        f'   {pp(b["coverage"] - a["coverage"]):+8.3f}   {a["executions"]:10d}'
        f'   {b["executions"]:10d}')

  last = minutes[-1]
  a, b = base[last], after[last]
  diff = pp(b['coverage'] - a['coverage'])
  ahead = sum(1 for m in minutes if after[m]['coverage'] > base[m]['coverage'])
  print()
  print(f'At {last} min: coverage {pp(a["coverage"]):.3f}% -> '
        f'{pp(b["coverage"]):.3f}% ({diff:+.3f} pp), executions '
        f'{a["executions"]} -> {b["executions"]} '
        f'({b["executions"] / max(a["executions"], 1):.2f}x)')
  print(f'The after session had more coverage in {ahead} of {len(minutes)} '
        'minutes.')
  for key in ('corpus', 'crash_signatures', 'unique_crashes', 'timed_out',
              'flaky', 'infrastructure_failures'):
    print(f'  {key}: {a.get(key, "?")} -> {b.get(key, "?")}')
  if abs(diff) < NOISE_PP:
    print(f'The difference is below {NOISE_PP} points: it is probably noise.')
  else:
    print(f'The difference is {diff:+.3f} points. Repeat the run before you '
          'trust it.')
  return 0


if __name__ == '__main__':
  sys.exit(main())
