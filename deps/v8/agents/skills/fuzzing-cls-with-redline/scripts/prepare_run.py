#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Prepares, builds, stages, tests, and stops V8 Redline fuzzing runs.

Self-contained (Python stdlib only) so callers can run it locally or pipe it
over SSH (`python3 -`). Keeps build directories, GN flags, binary isolation,
nodump.so compilation, selftest gating, and scoped shutdown consistent.
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import re
import shutil
import signal
import socket
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

BASE_D8_GN_ARGS = [
    'is_debug=false',
    'dcheck_always_on=true',
    'v8_enable_debug_code=true',
    'v8_static_library=true',
    'v8_enable_verify_heap=true',
    'v8_redline=true',
    'v8_enable_backtrace=true',
    'v8_enable_partition_alloc=false',
    'symbol_level=1',
    'target_cpu="x64"',
]

VARIANT_GN_EXTRAS = {
    'asan_slow_dcheck': [
        'is_asan=true',
        'v8_enable_slow_dchecks=true',
        'sanitizer_coverage_flags="inline-bool-flag"',
    ],
    'counters_slow_dcheck': [
        'v8_enable_slow_dchecks=true',
        'sanitizer_coverage_flags="inline-8bit-counters,pc-table"',
    ],
    'counters_dcheck': [
        'sanitizer_coverage_flags="inline-8bit-counters,pc-table"',
    ],
    'slow_dcheck': [
        'v8_enable_slow_dchecks=true',
        'sanitizer_coverage_flags="inline-bool-flag"',
    ],
    'verif': [
        'v8_enable_verification_features=true',
        'sanitizer_coverage_flags="inline-bool-flag"',
    ],
    'arm64sim': [
        'v8_target_cpu="arm64"',
        'sanitizer_coverage_flags="inline-bool-flag"',
    ],
}

REDLINE_GN_ARGS = [
    'is_debug=false',
    'target_cpu="x64"',
    'use_allocator_shim=false',
]

ASAN_OPTIONS = ('abort_on_error=1:detect_leaks=0:allow_user_segv_handler=1:'
                'allocator_may_return_null=1')

NODUMP_C_SOURCE = """\
extern int prctl(int option, unsigned long arg2, unsigned long arg3,
                 unsigned long arg4, unsigned long arg5);
__attribute__((constructor)) static void redline_nodump(void) {
  prctl(4 /* PR_SET_DUMPABLE */, 0, 0, 0, 0);
}
"""

RE_RECLIENT_ERROR = re.compile(
    r'reclient|reproxy|remoteexec|\brbe\b|not authenticated|gcert|rewrapper|'
    r'reapi|credhelper',
    re.IGNORECASE,
)


def _variant_has_asan(text: str) -> bool:
  if not text:
    return False
  if re.search(r'(^|[_-])no(n)?[_-]?asan($|[_-])', text):
    return False
  return bool(re.search(r'(^|[_-])asan($|[_-])', text))


def _build_env() -> dict[str, str]:
  env = dict(os.environ)
  depot = os.path.join(os.path.expanduser('~'), 'depot_tools')
  path = env.get('PATH', '')
  if os.path.isdir(depot) and depot not in path.split(os.pathsep):
    env['PATH'] = f'{depot}{os.pathsep}{path}' if path else depot
  return env


def _read_cmdline(pid: int) -> str:
  try:
    with open(f'/proc/{pid}/cmdline', 'rb') as f:
      raw = f.read()
    return raw.replace(b'\x00', b' ').decode('utf-8', errors='replace').strip()
  except OSError:
    return ''


def _pid_alive(pid: int) -> bool:
  try:
    os.kill(pid, 0)
  except OSError:
    return False
  try:
    with open(f'/proc/{pid}/stat', encoding='utf-8', errors='replace') as f:
      raw = f.read()
    rparen = raw.rfind(')')
    if rparen != -1 and raw[rparen + 2:rparen + 3] == 'Z':
      return False
  except OSError:
    pass
  return True


def _ram_gb() -> int:
  try:
    with open('/proc/meminfo', encoding='utf-8', errors='replace') as f:
      for line in f:
        if line.startswith('MemTotal:'):
          kb = int(line.split()[1])
          return max(1, int(round(kb / (1024 * 1024))))
  except (OSError, ValueError):
    pass
  return 0


def _discover_active_runs(home: str) -> list[dict]:
  runs: list[dict] = []
  base = os.path.join(home, 'cl-fuzz')
  if not os.path.isdir(base):
    return runs
  for entry in sorted(os.listdir(base)):
    if not entry.startswith('run-'):
      continue
    run_dir = os.path.join(base, entry)
    if not os.path.isdir(run_dir):
      continue
    alive_arms: list[dict] = []
    for pid_path in sorted(
        glob.glob(os.path.join(run_dir, 'arms', '*', 'redline.pid'))):
      try:
        with open(pid_path, encoding='utf-8', errors='replace') as f:
          pid = int(f.read().strip())
      except (OSError, ValueError):
        continue
      if _pid_alive(pid) and 'redline' in _read_cmdline(pid):
        arm_name = os.path.basename(os.path.dirname(pid_path))
        alive_arms.append({'arm': arm_name, 'pid': pid})
    if alive_arms:
      runs.append({'run_root': run_dir, 'running_arms': alive_arms})
  return runs


def probe_host() -> dict:
  """Returns host capacity, environment details, and active CL fuzz runs."""
  home = os.path.expanduser('~')
  hostname = socket.getfqdn()
  vcpus = os.cpu_count() or 1
  is_cloudtop = (
      hostname.endswith('.c.googlers.com') or
      (home.startswith('/usr/local/google/home/') and vcpus >= 8))
  disk = shutil.disk_usage(home)
  free_disk_gb = round(disk.free / (1024**3), 1)
  env = _build_env()
  gn_path = shutil.which('gn', path=env.get('PATH', '')) or ''
  autoninja_path = shutil.which('autoninja', path=env.get('PATH', '')) or ''
  linger = 'unknown'
  user = os.environ.get('USER', '')
  if user and shutil.which('loginctl'):
    try:
      proc = subprocess.run(
          ['loginctl', 'show-user', user, '-p', 'Linger'],
          capture_output=True,
          text=True,
          check=False,
      )
      if proc.returncode == 0:
        for line in proc.stdout.splitlines():
          if line.startswith('Linger='):
            linger = line.split('=', 1)[1].strip()
    except OSError:
      pass
  return {
      'home': home,
      'hostname': hostname,
      'is_cloudtop': is_cloudtop,
      'vcpus': vcpus,
      'ram_gb': _ram_gb(),
      'free_disk_gb': free_disk_gb,
      'linger': linger,
      'gn': gn_path,
      'autoninja': autoninja_path,
      'active_runs': _discover_active_runs(home),
  }


def gn_args_for_variant(
    variant: str,
    builtins_coverage: bool = True,
    change_based: bool = False,
    extra_gn_args: list[str] | None = None,
    use_remoteexec: bool = True,
) -> list[str]:
  """Builds the GN argument list for one target d8 variant."""
  if variant not in VARIANT_GN_EXTRAS:
    known = ', '.join(sorted(VARIANT_GN_EXTRAS))
    raise ValueError(f'Unknown build_variant "{variant}" (known: {known})')

  args = list(BASE_D8_GN_ARGS)
  extras = list(VARIANT_GN_EXTRAS[variant])
  if variant == 'arm64sim' and change_based:
    extras = [
        'sanitizer_coverage_flags="inline-8bit-counters,pc-table"'
        if a.startswith('sanitizer_coverage_flags=') else a for a in extras
    ]
  args.extend(extras)
  args.append('v8_enable_builtins_profiling=' +
              ('true' if builtins_coverage else 'false'))
  for raw in extra_gn_args or []:
    key = raw.split('=', 1)[0].strip()
    args = [a for a in args if a.split('=', 1)[0].strip() != key]
    args.append(raw)
  if use_remoteexec:
    args.append('use_remoteexec=true')
  return args


def collect_variant_specs(plan: dict) -> list[dict]:
  """Deduplicates planned d8 build variants while merging per-arm GN options."""
  by_variant: dict[str, dict] = {}
  for arm in plan.get('arms', []):
    variant = str(arm.get('build_variant', '')).strip()
    if variant not in VARIANT_OUT_DIRS:
      known = ', '.join(sorted(VARIANT_OUT_DIRS))
      raise ValueError(f'Unknown build_variant "{variant}" (known: {known})')
    spec = by_variant.setdefault(
        variant, {
            'variant': variant,
            'out_dir': VARIANT_OUT_DIRS[variant],
            'builtins_coverage': False,
            'change_based': False,
            'extra_gn_args': [],
        })
    if arm.get('builtins_coverage', True):
      spec['builtins_coverage'] = True
    if arm.get('change_based', False):
      spec['change_based'] = True
    for gn_arg in arm.get('extra_gn_args', []) or []:
      if gn_arg not in spec['extra_gn_args']:
        spec['extra_gn_args'].append(str(gn_arg))
  return list(by_variant.values())


def _run_gn_and_ninja(
    v8_dir: str,
    out_name: str,
    target: str,
    gn_args: list[str],
    use_remoteexec: bool,
    env: dict[str, str],
) -> None:
  out_rel = os.path.join('out', out_name)
  attempts = [True, False] if use_remoteexec else [False]
  for idx, remote in enumerate(attempts):
    cur_args = [a for a in gn_args if not a.startswith('use_remoteexec=')]
    if remote:
      cur_args.append('use_remoteexec=true')
    gn_cmd = ['gn', 'gen', out_rel, '--args=' + ' '.join(cur_args)]
    gn_proc = subprocess.run(
        gn_cmd,
        cwd=v8_dir,
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    if gn_proc.returncode != 0:
      combined = (gn_proc.stderr or '') + '\n' + (gn_proc.stdout or '')
      if remote and idx + 1 < len(attempts) and RE_RECLIENT_ERROR.search(
          combined):
        print(
            f'Remoteexec unavailable during gn gen {out_rel}; '
            'retrying with local build.',
            file=sys.stderr,
        )
        continue
      tail = '\n'.join(combined.strip().splitlines()[-20:])
      raise RuntimeError(f'gn gen failed for {out_rel}:\n{tail}')

    ninja_cmd = ['autoninja', '-C', out_rel, target]
    if not remote:
      cpus = str(os.cpu_count() or 4)
      ninja_cmd = ['autoninja', '-j', cpus, '-l', cpus, '-C', out_rel, target]
    ninja_proc = subprocess.run(
        ninja_cmd,
        cwd=v8_dir,
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    if ninja_proc.returncode == 0:
      return
    combined = (ninja_proc.stderr or '') + '\n' + (ninja_proc.stdout or '')
    if remote and idx + 1 < len(attempts) and RE_RECLIENT_ERROR.search(
        combined):
      print(
          f'Remoteexec failed during autoninja {out_rel}; '
          'retrying with local build.',
          file=sys.stderr,
      )
      continue
    tail = '\n'.join(combined.strip().splitlines()[-30:])
    raise RuntimeError(f'autoninja failed for {out_rel} {target}:\n{tail}')


def build_plan(v8_dir: str, plan_path: str, use_remoteexec: bool) -> dict:
  """Builds Redline and every planned d8 variant."""
  v8_dir = os.path.abspath(v8_dir)
  with open(plan_path, encoding='utf-8') as f:
    plan = json.load(f)
  specs = collect_variant_specs(plan)
  if not specs:
    raise RuntimeError(f'No arms found in {plan_path}')
  env = _build_env()
  _run_gn_and_ninja(v8_dir, 'rl_bin', 'redline', list(REDLINE_GN_ARGS),
                    use_remoteexec, env)
  built: list[dict] = [{'target': 'redline', 'out_dir': 'out/rl_bin'}]
  for spec in specs:
    gn_args = gn_args_for_variant(
        spec['variant'],
        builtins_coverage=spec['builtins_coverage'],
        change_based=spec['change_based'],
        extra_gn_args=spec['extra_gn_args'],
        use_remoteexec=use_remoteexec,
    )
    _run_gn_and_ninja(v8_dir, spec['out_dir'], 'd8', gn_args, use_remoteexec,
                      env)
    built.append({
        'target': 'd8',
        'variant': spec['variant'],
        'out_dir': os.path.join('out', spec['out_dir']),
    })
  return {'built': built}


def _copy_if_exists(src: str, dst_dir: str) -> None:
  if os.path.exists(src):
    shutil.copy2(src, os.path.join(dst_dir, os.path.basename(src)))


def _compile_nodump(v8_dir: str, run_root: str) -> str:
  c_path = os.path.join(run_root, 'nodump.c')
  so_path = os.path.join(run_root, 'nodump.so')
  with open(c_path, 'w', encoding='utf-8') as f:
    f.write(NODUMP_C_SOURCE)
  clang = os.path.join(v8_dir, 'third_party', 'llvm-build', 'Release+Asserts',
                       'bin', 'clang')
  if not os.path.exists(clang):
    clang = shutil.which('clang') or ''
  if not clang:
    raise RuntimeError('clang binary not found for nodump.so compilation')
  proc = subprocess.run(
      [
          clang,
          '-shared',
          '-fPIC',
          '-O2',
          '-nostdlib',
          '-fuse-ld=lld',
          c_path,
          '-o',
          so_path,
      ],
      capture_output=True,
      text=True,
      check=False,
  )
  if proc.returncode != 0:
    raise RuntimeError(f'nodump.so compile failed:\n{proc.stderr.strip()}')
  return so_path


def _selftest_passes(stdout: str, returncode: int, is_asan: bool) -> bool:
  if returncode == 0:
    return True
  if not is_asan:
    return False
  # Under heavy load on ASan builds, only the 2 s amplified timing check can
  # fail intermittently. Require every other check to pass.
  fail_lines = [
      ln.strip() for ln in stdout.splitlines() if ln.startswith('FAIL')
  ]
  if not fail_lines:
    return False
  return all('amplified' in ln.lower() for ln in fail_lines)


def stage_and_selftest(
    v8_dir: str,
    run_root: str,
    plan_path: str,
    skip_selftest: bool = False,
) -> dict:
  """Copies built binaries into run_root/bin, compiles nodump.so, and tests."""
  v8_dir = os.path.abspath(v8_dir)
  run_root = os.path.abspath(run_root)
  with open(plan_path, encoding='utf-8') as f:
    plan = json.load(f)
  specs = collect_variant_specs(plan)
  if not specs:
    raise RuntimeError(f'No arms found in {plan_path}')

  os.makedirs(run_root, exist_ok=True)
  rl_src = os.path.join(v8_dir, 'out', 'rl_bin')
  rl_bin_src = os.path.join(rl_src, 'redline')
  if not os.path.exists(rl_bin_src):
    raise RuntimeError(f'Missing built redline binary: {rl_bin_src}')

  rl_dst = os.path.join(run_root, 'bin', 'redline_bundle')
  os.makedirs(rl_dst, exist_ok=True)
  for name in ('redline', 'snapshot_blob.bin', 'icudtl.dat'):
    _copy_if_exists(os.path.join(rl_src, name), rl_dst)
  for data_dir in ('presets', 'testdata'):
    src_dir = os.path.join(v8_dir, 'experimental', 'redline', data_dir)
    dst_dir = os.path.join(rl_dst, data_dir)
    if os.path.isdir(src_dir):
      shutil.copytree(src_dir, dst_dir, dirs_exist_ok=True)

  staged_variants: dict[str, str] = {}
  for spec in specs:
    variant = spec['variant']
    out_dir = os.path.join(v8_dir, 'out', spec['out_dir'])
    d8_src = os.path.join(out_dir, 'd8')
    if not os.path.exists(d8_src):
      raise RuntimeError(f'Missing built d8 for {variant}: {d8_src}')
    var_dst = os.path.join(run_root, 'bin', variant)
    os.makedirs(var_dst, exist_ok=True)
    for name in ('d8', 'snapshot_blob.bin', 'icudtl.dat'):
      _copy_if_exists(os.path.join(out_dir, name), var_dst)
    for so_file in glob.glob(os.path.join(out_dir, '*.so')):
      _copy_if_exists(so_file, var_dst)
    staged_variants[variant] = os.path.join(var_dst, 'd8')

  nodump_so = _compile_nodump(v8_dir, run_root)
  staged_redline = os.path.join(rl_dst, 'redline')

  selftest_results: list[dict] = []
  if not skip_selftest:
    off = subprocess.run(
        [staged_redline, 'selftest', '--offline'],
        capture_output=True,
        text=True,
        check=False,
    )
    if off.returncode != 0:
      raise RuntimeError('redline selftest --offline failed:\n' +
                         (off.stdout + '\n' + off.stderr).strip())
    selftest_results.append({'target': 'offline', 'ok': True})

    for variant, d8_path in staged_variants.items():
      is_asan = _variant_has_asan(variant)
      env_val = f'LD_PRELOAD={nodump_so}'
      if is_asan:
        env_val += f',ASAN_OPTIONS={ASAN_OPTIONS}'
      proc = subprocess.run(
          [
              staged_redline,
              'selftest',
              f'--d8={d8_path}',
              f'--d8-env={env_val}',
          ],
          capture_output=True,
          text=True,
          check=False,
      )
      if not _selftest_passes(proc.stdout, proc.returncode, is_asan):
        raise RuntimeError(f'redline selftest failed for {variant}:\n' +
                           (proc.stdout + '\n' + proc.stderr).strip())
      selftest_results.append({
          'target': variant,
          'ok': True,
          'returncode': proc.returncode,
      })

  return {
      'redline': staged_redline,
      'nodump_so': nodump_so,
      'variants': staged_variants,
      'selftest': selftest_results,
  }


def _has_end_event(arm_dir: str) -> bool:
  events_path = os.path.join(arm_dir, 'events.jsonl')
  if not os.path.exists(events_path):
    return False
  ended = False
  try:
    with open(events_path, encoding='utf-8', errors='replace') as f:
      for line in f:
        try:
          ev = json.loads(line)
        except ValueError:
          continue
        kind = ev.get('event')
        if kind == 'start':
          ended = False
        elif kind == 'end':
          ended = True
  except OSError:
    return False
  return ended


def _find_scoped_pids(bin_prefix: str) -> list[int]:
  matches: list[int] = []
  if not os.path.isdir('/proc'):
    return matches
  my_pid = os.getpid()
  for entry in os.listdir('/proc'):
    if not entry.isdigit():
      continue
    pid = int(entry)
    if pid == my_pid:
      continue
    cmdline = _read_cmdline(pid)
    if bin_prefix in cmdline and _pid_alive(pid):
      matches.append(pid)
  return matches


def stop_run(run_root: str, timeout_sec: float = 60.0) -> dict:
  """Sends SIGINT to active arms, waits for exit, and kills stray d8 workers."""
  run_root = os.path.abspath(run_root)
  pid_files = sorted(
      glob.glob(os.path.join(run_root, 'arms', '*', 'redline.pid')))
  signaled: list[tuple[str, int, str]] = []
  for pid_path in pid_files:
    arm_dir = os.path.dirname(pid_path)
    arm_name = os.path.basename(arm_dir)
    try:
      with open(pid_path, encoding='utf-8', errors='replace') as f:
        pid = int(f.read().strip())
    except (OSError, ValueError):
      continue
    if not _pid_alive(pid):
      continue
    cmdline = _read_cmdline(pid)
    if 'redline' not in cmdline:
      continue
    try:
      os.kill(pid, signal.SIGINT)
      signaled.append((arm_name, pid, arm_dir))
    except OSError:
      continue

  deadline = time.monotonic() + max(0.0, timeout_sec)
  while time.monotonic() < deadline:
    still_up = [p for _, p, _ in signaled if _pid_alive(p)]
    if not still_up:
      break
    time.sleep(min(1.0, max(0.05, deadline - time.monotonic())))

  bin_prefix = os.path.join(run_root, 'bin') + os.sep
  stray_pids = _find_scoped_pids(bin_prefix)
  killed: list[int] = []
  if stray_pids:
    for pid in stray_pids:
      try:
        os.kill(pid, signal.SIGTERM)
      except OSError:
        pass
    time.sleep(min(5.0, max(0.0, timeout_sec)))
    for pid in _find_scoped_pids(bin_prefix):
      try:
        os.kill(pid, signal.SIGKILL)
        killed.append(pid)
      except OSError:
        pass

  arms_status = [{
      'arm': name,
      'pid': pid,
      'exited': not _pid_alive(pid),
      'clean_end_event': _has_end_event(arm_dir),
  } for name, pid, arm_dir in signaled]
  return {
      'run_root': run_root,
      'signaled': arms_status,
      'force_killed_strays': killed,
  }


def main(argv: list[str]) -> int:
  parser = argparse.ArgumentParser(
      description='Probe, build, stage, selftest, and stop CL fuzzing runs.')
  sub = parser.add_subparsers(dest='cmd', required=True)

  sub.add_parser('probe', help='Inspect host capacity and active runs')

  p_build = sub.add_parser('build', help='Build Redline and planned d8 arms')
  p_build.add_argument('--v8-dir', required=True, help='V8 checkout root')
  p_build.add_argument('--plan', required=True, help='Path to plan.json')
  p_build.add_argument(
      '--no-remoteexec',
      action='store_true',
      help='Build locally without use_remoteexec=true',
  )

  p_stage = sub.add_parser(
      'stage', help='Isolate binaries, compile nodump.so, and run selftest')
  p_stage.add_argument('--v8-dir', required=True, help='V8 checkout root')
  p_stage.add_argument('--run-root', required=True, help='Run root directory')
  p_stage.add_argument('--plan', required=True, help='Path to plan.json')
  p_stage.add_argument(
      '--skip-selftest',
      action='store_true',
      help='Stage binaries without running redline selftest',
  )

  p_stop = sub.add_parser('stop', help='Stop all arms under a run root')
  p_stop.add_argument('--run-root', required=True, help='Run root directory')
  p_stop.add_argument(
      '--timeout',
      type=float,
      default=60.0,
      help='Seconds to wait for graceful exit before stopping stray workers',
  )

  args = parser.parse_args(argv)
  try:
    if args.cmd == 'probe':
      res = probe_host()
    elif args.cmd == 'build':
      res = build_plan(args.v8_dir, args.plan, not args.no_remoteexec)
    elif args.cmd == 'stage':
      res = stage_and_selftest(args.v8_dir, args.run_root, args.plan,
                               args.skip_selftest)
    else:
      res = stop_run(args.run_root, args.timeout)
  except (OSError, RuntimeError, ValueError) as err:
    print(f'Error: {err}', file=sys.stderr)
    return 1

  print(json.dumps(res, indent=2))
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
