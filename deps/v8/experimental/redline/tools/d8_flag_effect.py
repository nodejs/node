#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Checks that d8 accepts Redline's random flag draws without silent changes.

For each preset (`maglev`, `turbolev`), runs one baseline command:

  d8 <preset d8 args> --print-flag-values -e ""

Then draws `N` flag sets from `redline flags` and runs each draw in the
production argument order:

  d8 <draw> <preset d8 args> --print-flag-values -e ""

Reports contradictions, flags that `d8` silently changed, boolean flags that
have no effect on the build, and value flags that drew their baseline default.
"""

import argparse
import collections
import concurrent.futures
import json
import os
import re
import subprocess
import sys

V8_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..'))
DEFAULT_REDLINE = os.path.join(V8_ROOT, 'out', 'redline', 'redline')
DEFAULT_D8 = os.environ.get('REDLINE_D8',
                            os.path.join(V8_ROOT, 'out', 'fuzzbuild', 'd8'))
DEFAULT_PRESETS = ('maglev', 'turbolev')
DEFAULT_TIMEOUT_SEC = 30.0
D8_SHELL_ONLY_FLAGS = frozenset(('omit-quit', 'expose-fast-api'))
CONTRADICTION_LINE = re.compile(
    r'(Flag processing error:.*|.*Contradictory.*|'
    r'Warning: resetting flag .*|Error: .*|.*Cycle in flag implications.*)')


def parse_flag_token(token):
  """Returns `(canonical_key, value_or_bool, raw_token)` for `--...`."""
  if not token.startswith('--'):
    return None
  body = token[2:]
  if '=' in body:
    name, val = body.split('=', 1)
    cleaned = val.strip('"')
    if cleaned == 'true':
      parsed_val = True
    elif cleaned == 'false':
      parsed_val = False
    else:
      parsed_val = cleaned
    return name.replace('_', '-'), parsed_val, token
  if body.startswith('no-'):
    return body[3:].replace('_', '-'), False, token
  if body.startswith('no_'):
    return body[3:].replace('_', '-'), False, token
  return body.replace('_', '-'), True, token


def parse_printed_flags(stdout_text):
  """Returns `{canonical_key: (value_or_bool, raw_line)}` from `d8` stdout."""
  printed = {}
  for line in stdout_text.splitlines():
    stripped = line.strip()
    if not stripped.startswith('--'):
      continue
    parsed = parse_flag_token(stripped)
    if parsed is not None:
      key, val, raw = parsed
      printed[key] = (val, raw)
  return printed


def extract_contradictions(returncode, stdout_text, stderr_text):
  """Returns a list of contradiction or error messages from one `d8` run."""
  issues = []
  combined = '\n'.join([stderr_text or '', stdout_text or ''])
  for line in combined.splitlines():
    stripped = line.strip()
    if CONTRADICTION_LINE.match(stripped):
      issues.append(stripped)
  if returncode != 0 and not issues:
    issues.append(f'd8 exited with status {returncode}')
  return issues


def check_one_run(base_args,
                  draw_args,
                  returncode,
                  stdout_text,
                  stderr_text,
                  baseline_printed=None):
  """Checks one `d8 --print-flag-values` run against the requested flags.

  Returns `(contradictions, changed_flags, noop_flags, default_draws)`.
  `changed_flags` lists `(requested_token, printed_token)` pairs. `noop_flags`
  lists drawn boolean flags that already had the same value in
  `baseline_printed`. `default_draws` lists drawn `--name=value` flags whose
  drawn value equals the value in `baseline_printed`.
  """
  contradictions = extract_contradictions(returncode, stdout_text, stderr_text)
  if returncode != 0:
    return contradictions, [], [], []

  printed = parse_printed_flags(stdout_text)
  draw_requested = {}
  for token in draw_args:
    parsed = parse_flag_token(token)
    if parsed is not None:
      key, val, raw = parsed
      draw_requested[key] = (val, raw)

  base_requested = {}
  for token in base_args:
    parsed = parse_flag_token(token)
    if parsed is not None:
      key, val, raw = parsed
      base_requested[key] = (val, raw)

  requested_items = list(draw_requested.items()) + [
      (k, v) for k, v in base_requested.items() if k not in draw_requested
  ]
  changed = []
  noops = []
  default_draws = []
  for key, (req_val, req_raw) in requested_items:
    if key in D8_SHELL_ONLY_FLAGS and key not in printed:
      if key in draw_requested and key in base_requested:
        if base_requested[key][0] == req_val:
          noops.append(req_raw)
      continue
    if key not in printed:
      changed.append((req_raw, '<missing>'))
      continue
    got_val, got_raw = printed[key]
    if got_val != req_val:
      changed.append((req_raw, got_raw))
    elif key in draw_requested and baseline_printed is not None:
      base_entry = baseline_printed.get(key)
      if base_entry is not None and base_entry[0] == got_val:
        if '=' in req_raw and not isinstance(req_val, bool):
          default_draws.append(req_raw)
        else:
          noops.append(req_raw)
  return contradictions, changed, noops, default_draws


def load_preset_base_args(redline_bin, preset, timeout_sec=DEFAULT_TIMEOUT_SEC):
  """Returns `<Redline default args> <preset args>` from `--dump-config`."""
  proc = subprocess.run(
      [redline_bin, 'fuzz', f'--preset={preset}', '--dump-config'],
      capture_output=True,
      text=True,
      check=True,
      timeout=timeout_sec)
  config = json.loads(proc.stdout)
  return config['d8_args']


def draw_flag_sets(redline_bin,
                   preset,
                   n,
                   seed,
                   timeout_sec=DEFAULT_TIMEOUT_SEC):
  """Returns `n` flag lists from `redline flags`."""
  proc = subprocess.run([
      redline_bin, 'flags', f'--preset={preset}', f'--n={n}', f'--seed={seed}'
  ],
                        capture_output=True,
                        text=True,
                        check=True,
                        timeout=timeout_sec)
  lines = proc.stdout.splitlines()
  return [line.split() for line in lines[:n]]


def run_d8_print_flags(d8_bin,
                       base_args,
                       draw_args,
                       timeout_sec=DEFAULT_TIMEOUT_SEC):
  """Runs `d8 <draw> <base_args> --print-flag-values -e ""`."""
  env = dict(os.environ)
  env.pop('SHM_ID', None)
  cmd = [d8_bin] + list(draw_args) + list(base_args) + [
      '--print-flag-values', '-e', ''
  ]
  try:
    proc = subprocess.run(
        cmd, env=env, capture_output=True, text=True, timeout=timeout_sec)
  except subprocess.TimeoutExpired:
    return -1, '', f'Error: d8 timed out after {timeout_sec:g}s'
  return proc.returncode, proc.stdout, proc.stderr


def evaluate_preset(d8_bin,
                    redline_bin,
                    preset,
                    n,
                    seed,
                    jobs=1,
                    timeout_sec=DEFAULT_TIMEOUT_SEC):
  """Runs the baseline and checks `n` flag draws for `preset`."""
  base_args = load_preset_base_args(redline_bin, preset, timeout_sec)
  base_rc, base_out, base_err = run_d8_print_flags(
      d8_bin, base_args, [], timeout_sec=timeout_sec)
  if base_rc != 0:
    detail = (base_err or base_out).strip()
    suffix = f': {detail}' if detail else ''
    raise RuntimeError(f'Baseline d8 run failed for preset {preset} '
                       f'(exit status {base_rc}){suffix}')
  baseline_printed = parse_printed_flags(base_out)
  if not baseline_printed:
    raise RuntimeError(
        f'Baseline d8 run printed no flag values for preset {preset}')
  draws = draw_flag_sets(redline_bin, preset, n, seed, timeout_sec)

  def task(draw):
    rc, out, err = run_d8_print_flags(
        d8_bin, base_args, draw, timeout_sec=timeout_sec)
    return check_one_run(base_args, draw, rc, out, err, baseline_printed)

  if jobs > 1:
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
      outcomes = list(pool.map(task, draws))
  else:
    outcomes = [task(draw) for draw in draws]

  contradictions = collections.Counter()
  changed = collections.Counter()
  noops = collections.Counter()
  default_draws = collections.Counter()
  for run_contradictions, run_changed, run_noops, run_defaults in outcomes:
    for item in run_contradictions:
      contradictions[item] += 1
    for req_raw, got_raw in run_changed:
      changed[(req_raw, got_raw)] += 1
    for noop_raw in run_noops:
      noops[noop_raw] += 1
    for default_raw in run_defaults:
      default_draws[default_raw] += 1

  return {
      'preset': preset,
      'draws': len(draws),
      'contradictions': dict(sorted(contradictions.items())),
      'changed': {
          f'{req} -> {got}': count
          for (req, got), count in sorted(changed.items())
      },
      'noops': dict(sorted(noops.items())),
      'default_draws': dict(sorted(default_draws.items())),
  }


def format_report(results):
  """Formats the preset check results as human-readable text."""
  lines = []
  for res in results:
    preset = res['preset']
    draws = res['draws']
    contradictions = res['contradictions']
    changed = res['changed']
    noops = res.get('noops', {})
    default_draws = res.get('default_draws', {})
    lines.append(f'Preset {preset} ({draws} draws): '
                 f'{len(contradictions)} contradiction kind(s), '
                 f'{len(changed)} silently changed flag(s), '
                 f'{len(noops)} no-op flag(s) on this build, '
                 f'{len(default_draws)} value draw(s) at baseline default')
    if contradictions:
      lines.append('  Contradictions:')
      for msg, count in contradictions.items():
        lines.append(f'    {msg} ({count}/{draws} draws)')
    if changed:
      lines.append('  Silently changed flags:')
      for transition, count in changed.items():
        lines.append(f'    {transition} ({count}/{draws} draws)')
    if noops:
      lines.append('  No-op on this build:')
      for flag, count in noops.items():
        lines.append(f'    {flag} ({count}/{draws} draws)')
    if default_draws:
      lines.append('  Value flags drawn with baseline value:')
      for flag, count in default_draws.items():
        lines.append(f'    {flag} ({count}/{draws} draws)')
  return '\n'.join(lines)


def main(argv=None):
  parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
  parser.add_argument('--d8', default=DEFAULT_D8)
  parser.add_argument('--redline', default=DEFAULT_REDLINE)
  parser.add_argument(
      '--presets',
      default=','.join(DEFAULT_PRESETS),
      help='comma-separated preset names (default: maglev,turbolev)')
  parser.add_argument('--n', type=int, default=100)
  parser.add_argument('--seed', type=int, default=1)
  parser.add_argument('--jobs', type=int, default=2)
  parser.add_argument(
      '--timeout',
      type=float,
      default=DEFAULT_TIMEOUT_SEC,
      help='timeout in seconds for each subprocess call (default: 30)')
  parser.add_argument(
      '--strict',
      action='store_true',
      help='exit with status 1 when a contradiction, silent change or no-op '
      'flag is found')
  args = parser.parse_args(argv)

  presets = [p.strip() for p in args.presets.split(',') if p.strip()]
  try:
    results = [
        evaluate_preset(
            args.d8,
            args.redline,
            preset,
            args.n,
            args.seed,
            jobs=args.jobs,
            timeout_sec=args.timeout) for preset in presets
    ]
  except (OSError, subprocess.SubprocessError, RuntimeError) as exc:
    print(f'd8_flag_effect.py: error: {exc}', file=sys.stderr)
    return 2
  print(format_report(results))
  has_issue = any(
      r['contradictions'] or r['changed'] or r.get('noops') for r in results)
  return 1 if (args.strict and has_issue) else 0


if __name__ == '__main__':
  sys.exit(main())
