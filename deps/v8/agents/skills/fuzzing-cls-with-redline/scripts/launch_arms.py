#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Prepares targets, writes arm_meta.json, and launches Redline arms detached.

Self-contained (Python stdlib only) so callers can run it locally or pipe it
over SSH (`python3 -`). Reads a plan JSON from analyze_diff.py and translates
each ArmSpec into exact `redline fuzz` CLI flags.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import time

VARIANT_OUT_DIRS = {
    'asan_slow_dcheck': 'rl_asan_slow',
    'counters_slow_dcheck': 'rl_counters',
    'counters_dcheck': 'rl_counters_fast',
    'slow_dcheck': 'rl_slow_dcheck',
    'verif': 'rl_verif',
    'arm64sim': 'rl_arm64sim',
}

VALID_FLAG_PROFILES = ('maglev', 'turbolev', 'fuzzilli')

ASAN_OPTIONS = ('abort_on_error=1:detect_leaks=0:allow_user_segv_handler=1:'
                'allocator_may_return_null=1')

RE_ARM_NAME = re.compile(r'^[A-Za-z0-9._-]+$')


def _variant_has_asan(text: str) -> bool:
  if not text:
    return False
  if re.search(r'(^|[_-])no(n)?[_-]?asan($|[_-])', text):
    return False
  return bool(re.search(r'(^|[_-])asan($|[_-])', text))


def _pid_alive_redline(pid_path: str) -> tuple[bool, int | None]:
  try:
    with open(pid_path, encoding='utf-8', errors='replace') as f:
      pid = int(f.read().strip())
  except (OSError, ValueError):
    return False, None
  try:
    os.kill(pid, 0)
  except OSError:
    return False, pid
  try:
    with open(f'/proc/{pid}/stat', encoding='utf-8', errors='replace') as f:
      raw = f.read()
    rparen = raw.rfind(')')
    if rparen != -1 and raw[rparen + 2:rparen + 3] == 'Z':
      return False, pid
  except OSError:
    pass
  try:
    with open(f'/proc/{pid}/cmdline', 'rb') as f:
      cmd = f.read().replace(b'\x00', b' ').decode('utf-8', errors='replace')
    if 'redline' not in cmd:
      return False, pid
  except OSError:
    pass
  return True, pid


def resolve_d8(v8_dir: str, run_root: str, variant: str) -> str:
  isolated = os.path.abspath(os.path.join(run_root, 'bin', variant, 'd8'))
  if os.path.exists(isolated):
    return isolated
  out_name = VARIANT_OUT_DIRS.get(variant, f'rl_{variant}')
  return os.path.abspath(os.path.join(v8_dir, 'out', out_name, 'd8'))


def resolve_redline(v8_dir: str, run_root: str) -> str:
  isolated = os.path.abspath(
      os.path.join(run_root, 'bin', 'redline_bundle', 'redline'))
  if os.path.exists(isolated):
    return isolated
  return os.path.abspath(os.path.join(v8_dir, 'out', 'rl_bin', 'redline'))


def validate_arm_schema(arm: dict) -> None:
  """Checks arm fields up front to prevent silent misconfiguration."""
  name = str(arm.get('name', ''))
  if not name or not RE_ARM_NAME.match(name) or name in ('.', '..'):
    raise ValueError(f'Invalid arm name: {name!r}')
  if 'extra_flags' in arm:
    raise ValueError(
        f'Arm {name}: use "d8_extra_args" in plan.json, not "extra_flags"')
  variant = arm.get('build_variant')
  if not isinstance(variant, str) or not variant.strip():
    raise ValueError(f'Arm {name}: missing non-empty "build_variant"')
  raw_extra = arm.get('d8_extra_args', [])
  if not isinstance(raw_extra, list) or not all(
      isinstance(x, str) for x in raw_extra):
    raise ValueError(f'Arm {name}: "d8_extra_args" must be a list of strings')
  raw_excluded = arm.get('excluded_flags', [])
  if raw_excluded is not None and (not isinstance(
      raw_excluded, list) or not all(isinstance(x, str) for x in raw_excluded)):
    raise ValueError(f'Arm {name}: "excluded_flags" must be a list of strings')
  raw_seeds = arm.get('seeds', '')
  if raw_seeds is not None and not isinstance(raw_seeds, (str, list)):
    raise ValueError(
        f'Arm {name}: "seeds" must be a comma-separated string or list')
  if arm.get('randomize_d8_flags'):
    profile = str(arm.get('d8_flag_profile', 'fuzzilli'))
    if profile not in VALID_FLAG_PROFILES:
      raise ValueError(f'Arm {name}: invalid d8_flag_profile {profile!r} '
                       f'(valid: {", ".join(VALID_FLAG_PROFILES)})')


def compute_targets(
    v8_dir: str,
    d8_path: str,
    base: str,
    head: str,
    out_path: str,
    edge_map_path: str = '',
) -> tuple[str, list[str]]:
  """Runs change_targets.py and returns (targets_file_or_empty, new_flags)."""
  script = os.path.join(v8_dir, 'experimental', 'redline', 'tools',
                        'change_targets.py')
  if not os.path.exists(script):
    print(
        f'WARNING: {script} not found. Running arm without --targets.',
        file=sys.stderr)
    return '', []
  out_dir = os.path.dirname(os.path.abspath(out_path))
  if out_dir:
    os.makedirs(out_dir, exist_ok=True)
  cmd = [
      sys.executable,
      script,
      f'--d8={d8_path}',
      f'--repo={v8_dir}',
      f'--base={base}',
      f'--head={head}',
      f'--out={out_path}',
  ]
  if edge_map_path:
    cmd.append(f'--edge-map={edge_map_path}')
  proc = subprocess.run(cmd, capture_output=True, text=True, check=False)
  if proc.returncode != 0:
    msg = (proc.stderr or proc.stdout).strip().splitlines()
    reason = msg[-1] if msg else f'exit {proc.returncode}'
    print(
        f'WARNING: change_targets.py did not produce targets ({reason}). '
        'Running arm without --targets.',
        file=sys.stderr,
    )
    return '', []
  try:
    with open(out_path, encoding='utf-8') as f:
      data = json.load(f)
  except (OSError, ValueError) as err:
    print(
        f'WARNING: unreadable {out_path} ({err}). Running without --targets.',
        file=sys.stderr)
    return '', []
  stats = data.get('stats', {})
  total = int(stats.get('line_targets', 0)) + int(
      stats.get('function_targets', 0))
  if total <= 0:
    print(
        f'WARNING: {out_path} mapped 0 line/function targets. '
        'Running arm without --targets.',
        file=sys.stderr,
    )
    return '', []
  extra_flags = [str(x) for x in data.get('flags', []) if isinstance(x, str)]
  return os.path.abspath(out_path), extra_flags


def _parse_seed_entries(raw_seeds: object) -> list[str]:
  if isinstance(raw_seeds, list):
    return [str(x).strip() for x in raw_seeds if str(x).strip()]
  text = str(raw_seeds or '').strip()
  if not text or text == 'generative':
    return []
  return [p.strip() for p in text.split(',') if p.strip()]


def build_fuzz_command(
    arm: dict,
    v8_dir: str,
    arm_dir: str,
    d8_path: str,
    redline_bin: str,
    nodump_so: str,
    targets_file: str,
) -> tuple[list[str], dict]:
  """Builds exact `redline fuzz` argv and the typed arm_meta.json dict."""
  validate_arm_schema(arm)
  variant = str(arm.get('build_variant', 'dcheck'))
  is_asan = (bool(arm['asan']) if 'asan' in arm else _variant_has_asan(variant))
  builtins_cov = bool(arm.get('builtins_coverage', True))
  extra_flags = [str(f) for f in arm.get('d8_extra_args', [])]
  if not builtins_cov and '--no-fuzzilli-enable-builtins-coverage' not in extra_flags:
    extra_flags.append('--no-fuzzilli-enable-builtins-coverage')

  duration_sec = int(arm.get('duration_sec', 0) or 0)

  cmd = [
      redline_bin,
      'fuzz',
      f'--d8={d8_path}',
      f'--preset={arm.get("preset", "default")}',
      f'--jobs={int(arm.get("jobs", 1))}',
      f'--timeout-ms={int(arm.get("timeout_ms", 600))}',
      '--max-execs-before-respawn=1000',
      '--stats-interval=300',
      '--no-component-stats',
      f'--out={arm_dir}',
  ]
  if duration_sec > 0:
    cmd.append(f'--duration={duration_sec}')

  seed_entries = _parse_seed_entries(arm.get('seeds', ''))
  resolved_imports: list[str] = []
  missing_imports: list[str] = []
  for rel in seed_entries:
    if rel == 'generative':
      continue
    candidate = rel if os.path.isabs(rel) else os.path.join(v8_dir, rel)
    if os.path.exists(candidate):
      resolved_imports.append(rel)
    else:
      missing_imports.append(rel)
  if missing_imports:
    print(
        f'WARNING: arm {arm.get("name")} skipped missing seed path(s): ' +
        ', '.join(missing_imports),
        file=sys.stderr,
    )
  if resolved_imports:
    cmd.append('--import=' + ','.join(resolved_imports))
    cmd.append(f'--import-root={v8_dir}')
  else:
    cmd.append('--prelude=none')

  weights = arm.get('generator_weights') or {}
  if isinstance(weights, dict) and weights:
    pairs = [f'{k}={v}' for k, v in sorted(weights.items())]
    cmd.append('--generator-weights=' + ','.join(pairs))

  if extra_flags:
    cmd.append('--d8-extra-args=' + ' '.join(extra_flags))

  if arm.get('randomize_d8_flags'):
    cmd.append('--randomize-d8-flags')
    profile = str(arm.get('d8_flag_profile', 'fuzzilli'))
    cmd.append(f'--d8-flag-profile={profile}')
  elif 'randomize_d8_flags' in arm:
    cmd.append('--no-randomize-d8-flags')

  env_entries: list[str] = []
  if nodump_so and os.path.exists(nodump_so):
    env_entries.append(f'LD_PRELOAD={nodump_so}')
  if is_asan:
    env_entries.append(f'ASAN_OPTIONS={ASAN_OPTIONS}')
  if env_entries:
    cmd.append('--d8-env=' + ','.join(env_entries))

  if targets_file:
    cmd.append(f'--targets={targets_file}')

  meta = {
      'name': str(arm.get('name', os.path.basename(arm_dir))),
      'build_variant': variant,
      'asan': is_asan,
      'builtins_coverage': builtins_cov,
      'change_based': bool(targets_file),
      'preset': str(arm.get('preset', 'default')),
      'extra_flags': extra_flags,
      'generator_weights': weights if isinstance(weights, dict) else {},
      'randomize_d8_flags': bool(arm.get('randomize_d8_flags', False)),
      'd8_flag_profile': str(arm.get('d8_flag_profile', 'fuzzilli')),
      'jobs': int(arm.get('jobs', 1)),
      'duration_sec': duration_sec,
      'timeout_ms': int(arm.get('timeout_ms', 600)),
      'seeds': ','.join(resolved_imports) if resolved_imports else 'generative',
      'd8_path': d8_path,
      'redline_bin': redline_bin,
      'nodump_so': nodump_so if nodump_so and os.path.exists(nodump_so) else '',
      'targets_file': targets_file,
  }
  return cmd, meta


def _tail_log(log_path: str, lines: int = 20) -> str:
  try:
    with open(log_path, encoding='utf-8', errors='replace') as f:
      content = f.read().splitlines()
    return '\n'.join(content[-lines:])
  except OSError:
    return ''


def launch(
    v8_dir: str,
    run_root: str,
    plan_path: str,
    base: str,
    head: str,
    dry_run: bool,
    only_stopped: bool = False,
    startup_wait_sec: float = 2.0,
) -> list[dict]:
  v8_dir = os.path.abspath(v8_dir)
  run_root = os.path.abspath(run_root)
  with open(plan_path, encoding='utf-8') as f:
    plan = json.load(f)

  arms = plan.get('arms', [])
  if not arms:
    raise RuntimeError(f'No arms in {plan_path}')

  redline_bin = resolve_redline(v8_dir, run_root)
  nodump_so = os.path.abspath(os.path.join(run_root, 'nodump.so'))
  if not dry_run and not os.path.exists(redline_bin):
    raise RuntimeError(f'Redline binary missing at {redline_bin}')

  # Validate every arm, binary, and existing PID before spawning any process.
  prepared: list[tuple[dict, str, str, str]] = []
  seen_names: set[str] = set()
  for raw_arm in arms:
    arm = dict(raw_arm)
    validate_arm_schema(arm)
    name = str(arm['name'])
    if name in seen_names:
      raise RuntimeError(f'Duplicate arm name in plan: {name}')
    seen_names.add(name)
    variant = str(arm['build_variant'])
    d8_path = resolve_d8(v8_dir, run_root, variant)
    if not dry_run and not os.path.exists(d8_path):
      raise RuntimeError(f'd8 binary for {name} ({variant}) missing: {d8_path}')
    arm_dir = os.path.abspath(os.path.join(run_root, 'arms', name))
    if not dry_run:
      alive, existing_pid = _pid_alive_redline(
          os.path.join(arm_dir, 'redline.pid'))
      if alive:
        if only_stopped:
          continue
        raise RuntimeError(
            f'Arm {name} is already running (PID {existing_pid}). '
            'Pass --only-stopped to restart only stopped arms.')
    prepared.append((arm, name, variant, d8_path))

  targets_cache: dict[str, tuple[str, list[str]]] = {}
  launched: list[dict] = []
  spawned_procs: list[tuple[str, subprocess.Popen, str]] = []

  for arm, name, variant, d8_path in prepared:
    arm_dir = os.path.abspath(os.path.join(run_root, 'arms', name))
    targets_file = ''
    if arm.get('change_based') and not dry_run:
      if d8_path not in targets_cache:
        t_out = os.path.join(run_root, f'targets_{variant}.json')
        out_name = VARIANT_OUT_DIRS.get(variant, f'rl_{variant}')
        out_dir = os.path.join(v8_dir, 'out', out_name)
        edge_map_path = (
            os.path.join(out_dir, 'redline', 'edges.jsonl.gz')
            if os.path.isdir(out_dir) else '')
        targets_cache[d8_path] = compute_targets(
            v8_dir,
            d8_path,
            base,
            head,
            t_out,
            edge_map_path=edge_map_path,
        )
      targets_file, new_flags = targets_cache[d8_path]
      excluded = set(arm.get('excluded_flags', []) or [])
      if new_flags:
        merged = list(arm.get('d8_extra_args', []))
        for nf in new_flags:
          negated = ('--no-' + nf[2:]) if nf.startswith('--') else ''
          if nf not in merged and negated not in merged and nf not in excluded:
            merged.append(nf)
        arm['d8_extra_args'] = merged

    cmd, meta = build_fuzz_command(arm, v8_dir, arm_dir, d8_path, redline_bin,
                                   nodump_so, targets_file)

    if dry_run:
      launched.append({'name': name, 'cmd': cmd, 'meta': meta})
      continue

    os.makedirs(arm_dir, exist_ok=True)
    with open(
        os.path.join(arm_dir, 'arm_meta.json'), 'w', encoding='utf-8') as f:
      json.dump(meta, f, indent=2)

    log_path = os.path.join(arm_dir, 'redline.log')
    with open(log_path, 'w', encoding='utf-8') as log_f:
      proc = subprocess.Popen(
          cmd,
          cwd=v8_dir,
          stdin=subprocess.DEVNULL,
          stdout=log_f,
          stderr=subprocess.STDOUT,
          start_new_session=True,
          close_fds=True,
      )
    with open(
        os.path.join(arm_dir, 'redline.pid'), 'w', encoding='utf-8') as pf:
      pf.write(f'{proc.pid}\n')
    spawned_procs.append((name, proc, log_path))
    launched.append({'name': name, 'pid': proc.pid, 'arm_dir': arm_dir})

  if not dry_run and spawned_procs and startup_wait_sec > 0:
    time.sleep(startup_wait_sec)
    failed_msgs: list[str] = []
    for name, proc, log_path in spawned_procs:
      rc = proc.poll()
      if isinstance(rc, int):
        tail = _tail_log(log_path)
        failed_msgs.append(
            f'Arm {name} exited immediately (code {rc}):\n{tail}')
    if failed_msgs:
      raise RuntimeError('\n\n'.join(failed_msgs))

  return launched


def main(argv: list[str]) -> int:
  parser = argparse.ArgumentParser(
      description='Launch Redline fuzzing arms from analyze_diff.py plan JSON.')
  parser.add_argument('--v8-dir', required=True, help='V8 checkout root')
  parser.add_argument('--run-root', required=True, help='Run output root')
  parser.add_argument('--plan', required=True, help='Path to plan.json')
  parser.add_argument('--base', default='origin/main', help='Diff base ref')
  parser.add_argument('--head', default='HEAD', help='Diff head ref')
  parser.add_argument('--dry-run', action='store_true', help='Print commands')
  parser.add_argument(
      '--only-stopped',
      action='store_true',
      help='Skip arms that already have a running Redline PID',
  )
  parser.add_argument(
      '--startup-wait-sec',
      type=float,
      default=2.0,
      help='Seconds to wait after spawn to catch immediate exits',
  )
  args = parser.parse_args(argv)

  try:
    result = launch(
        args.v8_dir,
        args.run_root,
        args.plan,
        args.base,
        args.head,
        args.dry_run,
        only_stopped=args.only_stopped,
        startup_wait_sec=args.startup_wait_sec,
    )
  except (OSError, RuntimeError, ValueError) as err:
    print(f'Error: {err}', file=sys.stderr)
    return 1

  print(json.dumps(result, indent=2))
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
