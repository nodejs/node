#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Reports immediate setup and hourly fuzzing/bug status for all running arms.

Self-contained (Python stdlib only) so callers can run it locally or pipe it
over SSH (`python3 -`). Reads events.jsonl, crashes/signatures.txt,
crashes/*.txt, redline.log, targets.json, arm_meta.json, and redline.pid.
Reports all crash classes including deterministic, state-dependent (using
`d8 --bundle-reprl`), amplified, flaky, and in-flight crashes.
"""

from __future__ import annotations

import argparse
import collections
import glob
import json
import os
import re
import sys

RE_LOG_LINE_TARGET = re.compile(r'\(line (\d+)/(\d+)\)')
RE_INFLIGHT_CRASH = re.compile(
    r'worker (\d+): crash #(\d+) of signature "(.+)"(?:\s+\(|$)')


def _read_json(path: str) -> dict:
  try:
    with open(path, encoding='utf-8', errors='replace') as f:
      data = json.load(f)
      return data if isinstance(data, dict) else {}
  except (OSError, ValueError):
    return {}


def _pid_alive(pid_path: str) -> tuple[bool, int | None]:
  try:
    with open(pid_path, encoding='utf-8', errors='replace') as f:
      pid = int(f.read().strip())
  except (OSError, ValueError):
    return False, None
  try:
    os.kill(pid, 0)
  except OSError:
    return False, pid
  # Check /proc/<pid>/stat on Linux to avoid counting a zombie as alive.
  stat_path = f'/proc/{pid}/stat'
  try:
    with open(stat_path, encoding='utf-8', errors='replace') as f:
      raw = f.read()
    rparen = raw.rfind(')')
    if rparen != -1:
      state = raw[rparen + 2:rparen + 3]
      if state == 'Z':
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


def _parse_report_txt(path: str) -> dict[str, str]:
  fields: dict[str, str] = {}
  stderr_lines: list[str] = []
  in_stderr = False
  try:
    with open(path, encoding='utf-8', errors='replace') as f:
      for line in f:
        stripped = line.rstrip('\n')
        if stripped == '--- stderr ---':
          in_stderr = True
          continue
        if in_stderr:
          if stripped.startswith('--- ') and stripped.endswith(' ---'):
            in_stderr = False
            continue
          if len(stderr_lines) < 8:
            stderr_lines.append(stripped)
          continue
        if ':' in stripped:
          key, _, val = stripped.partition(':')
          fields[key.strip()] = val.strip()
  except OSError:
    pass
  if stderr_lines:
    fields['_stderr_head'] = '\n'.join(stderr_lines)
  return fields


def _read_repro_run_line(path: str) -> str:
  try:
    with open(path, encoding='utf-8', errors='replace') as f:
      for _ in range(5):
        line = f.readline()
        if not line:
          break
        if line.startswith('// run: '):
          return line[len('// run: '):].strip()
  except OSError:
    pass
  return ''


def _format_duration(seconds: float) -> str:
  total = max(0, int(seconds))
  h, rem = divmod(total, 3600)
  m, s = divmod(rem, 60)
  return f'{h:02d}:{m:02d}:{s:02d}'


CLASSIFICATION_RANK = {
    'deterministic': 4,
    'state-dependent': 3,
    'amplified': 2,
    'flaky': 1,
    'in-flight (triaging)': 0,
}

PRESET_V8_LABELS = {
    'maglev': 'Maglev JIT',
    'turbolev': 'Turbolev / Turboshaft',
    'default': 'Default JS / TurboFan',
    'wasm': 'WebAssembly',
    'builtins': 'JS Builtins / Torque',
    'parser': 'Parser / Bytecode',
    'runtime': 'Interpreter / Objects / IC',
    'fuzzilli': 'General V8 JIT',
}


def _classify_crash(ev: dict, report: dict[str, str]) -> str:
  if ev.get('state_dependent') or report.get('state_dependent') == 'yes':
    return 'state-dependent'
  if ev.get('reproducible') or report.get('reproducible') == 'yes':
    return 'deterministic'
  # Real Redline emits amplified_args even when amplify is "no", so require an
  # explicit positive amplify/amplified marker.
  if ev.get('amplify') == 'yes' or report.get('amplified') == 'yes':
    return 'amplified'
  return 'flaky'


ASAN_ENV_PREFIX = ('ASAN_OPTIONS=abort_on_error=1:detect_leaks=0:'
                   'allow_user_segv_handler=1:allocator_may_return_null=1')


def _md_cell(value: object) -> str:
  return str(value).replace('\r', ' ').replace('\n', ' ').replace('|', '\\|')


def _variant_has_asan(text: str) -> bool:
  if not text:
    return False
  if re.search(r'(^|[_-])no(n)?[_-]?asan($|[_-])', text):
    return False
  return bool(re.search(r'(^|[_-])asan($|[_-])', text))


_UNSET_REPORT_VALS = ('', 'no', 'skipped', None)


def _wrap_env_prefix(cmd: str, is_asan: bool) -> str:
  # Omit LD_PRELOAD=nodump.so from user reproduction commands so gdb and local
  # runs work without PR_SET_DUMPABLE=0.
  if not is_asan or cmd.startswith('#'):
    return cmd
  return f'{ASAN_ENV_PREFIX} {cmd}'


def _build_repro_command(
    classification: str,
    d8_path: str,
    base_d8_args: list[str],
    ev: dict,
    report: dict[str, str],
    crashes_dir: str,
    redline_bin: str,
    is_asan: bool,
    nodump_so: str,
) -> tuple[str, list[str]]:
  """Returns (ready-to-run command, [notable artifact paths])."""
  artifacts: list[str] = []
  base_flags = list(base_d8_args)
  random_args = (ev.get('d8_random_args') or report.get('d8_random_args') or
                 '').strip()
  if random_args:
    for tok in random_args.split():
      if tok not in base_flags:
        base_flags.append(tok)

  repro_path = ev.get('repro') or ''
  if not repro_path and report.get('repro') not in _UNSET_REPORT_VALS:
    repro_path = os.path.join(crashes_dir, report['repro'])
  bundle_min = ev.get('bundle_min') or ''
  if not bundle_min and report.get('bundle_min') not in _UNSET_REPORT_VALS:
    bundle_min = os.path.join(crashes_dir, report['bundle_min'])
  bundle_path = ev.get('bundle') or ''
  if not bundle_path and report.get('bundle') not in _UNSET_REPORT_VALS:
    bundle_path = os.path.join(crashes_dir, report['bundle'])
  crash_js = ev.get('path') or ''

  for p in (repro_path, bundle_min, bundle_path, crash_js):
    if p and p not in artifacts:
      artifacts.append(p)

  def _promote_artifact(primary: str) -> None:
    if not primary:
      return
    if primary in artifacts:
      artifacts.remove(primary)
    artifacts.insert(0, primary)

  def _normalize_run_line(run_line: str, target_file: str = '') -> str:
    tokens = run_line.split()
    if tokens and d8_path and os.path.isabs(d8_path) and not os.path.isabs(
        tokens[0]):
      tokens[0] = d8_path
    if target_file and tokens and tokens[-1] == os.path.basename(target_file):
      tokens[-1] = target_file
    return ' '.join(tokens)

  if classification == 'state-dependent':
    chosen_bundle = bundle_min or bundle_path
    if chosen_bundle:
      _promote_artifact(chosen_bundle)
      run_line = _read_repro_run_line(chosen_bundle)
      if run_line:
        run_line = _normalize_run_line(run_line, chosen_bundle)
        if '--bundle-reprl' in run_line:
          return _wrap_env_prefix(run_line, is_asan), artifacts
      flag_part = f'{" ".join(base_flags)} ' if base_flags else ''
      raw = f'{d8_path} {flag_part}--bundle-reprl {chosen_bundle}'.strip()
      return _wrap_env_prefix(raw, is_asan), artifacts
    _promote_artifact(crash_js)
    history_dir = (crash_js[:-3] +
                   '.history') if crash_js.endswith('.js') else ''
    env_entries: list[str] = []
    if is_asan:
      env_entries.append(ASAN_ENV_PREFIX)
    if nodump_so:
      env_entries.append(f'LD_PRELOAD={nodump_so}')
    env_flag = f'--d8-env={",".join(env_entries)} ' if env_entries else ''
    args_flag = f'--d8-args="{" ".join(base_flags)}" ' if base_flags else ''
    hist_part = f'{history_dir}/*.js ' if history_dir else ''
    target_js = crash_js or '<crash.js>'
    return (
        f'{redline_bin} exec --d8={d8_path} '
        f'{env_flag}{args_flag}{hist_part}{target_js}',
        artifacts,
    )

  if classification == 'deterministic':
    if repro_path and os.path.exists(repro_path):
      run_line = _read_repro_run_line(repro_path)
      if run_line:
        _promote_artifact(repro_path)
        return _wrap_env_prefix(
            _normalize_run_line(run_line, repro_path), is_asan), artifacts
    target_js = repro_path or crash_js
    _promote_artifact(target_js)
    flags = ev.get('repro_flags') or report.get('repro_flags') or ' '.join(
        base_flags)
    raw = ' '.join(p for p in (d8_path, flags, target_js) if p)
    return _wrap_env_prefix(raw, is_asan), artifacts

  if classification == 'amplified':
    if repro_path and os.path.exists(repro_path):
      run_line = _read_repro_run_line(repro_path)
      if run_line:
        _promote_artifact(repro_path)
        return _wrap_env_prefix(
            _normalize_run_line(run_line, repro_path), is_asan), artifacts
    amp_args = ev.get('amplified_args') or report.get('amplified_args') or ''
    amp_file = report.get('amplified_program') or ''
    target_js = (
        os.path.join(crashes_dir, amp_file)
        if amp_file and amp_file != 'same' else crash_js)
    _promote_artifact(target_js)
    raw = ' '.join(
        p for p in (d8_path, ' '.join(base_flags), amp_args, target_js) if p)
    return _wrap_env_prefix(raw, is_asan), artifacts

  # Flaky or non-reproduced crash: retry loop checks for signal exits (> 128)
  # so normal uncaught JS exceptions (exit code 1) do not stop the loop.
  _promote_artifact(crash_js)
  target_js = crash_js or '<crash.js>'
  flag_str = ' '.join(base_flags)
  raw_inner = ' '.join(p for p in (d8_path, flag_str, target_js) if p)
  inner = _wrap_env_prefix(raw_inner, is_asan)
  cmd = (f'for i in $(seq 1 200); do {inner} >/tmp/v8_repro.log 2>&1; '
         'rc=$?; if [ "$rc" -gt 128 ]; then '
         'echo "crashed (exit $rc) on run $i"; '
         'tail -n 30 /tmp/v8_repro.log; break; fi; done')
  return cmd, artifacts


def collect_arm(arm_dir: str) -> dict:
  """Collects full status and all crash details for one arm directory."""
  meta = _read_json(os.path.join(arm_dir, 'arm_meta.json'))
  name = meta.get('name') or os.path.basename(os.path.abspath(arm_dir))
  alive, pid = _pid_alive(os.path.join(arm_dir, 'redline.pid'))

  events_path = os.path.join(arm_dir, 'events.jsonl')
  start_ev: dict = {}
  import_done_ev: dict = {}
  end_ev: dict = {}
  stats_history: list[dict] = []
  crash_events: list[dict] = []

  if os.path.exists(events_path):
    with open(events_path, encoding='utf-8', errors='replace') as f:
      for line in f:
        line = line.strip()
        if not line:
          continue
        try:
          ev = json.loads(line)
        except ValueError:
          continue
        kind = ev.get('event')
        if kind == 'start':
          # Reset per-session state when a restarted arm appends to events.jsonl.
          start_ev = ev
          import_done_ev = {}
          end_ev = {}
          stats_history = []
        elif kind == 'import_done':
          import_done_ev = ev
        elif kind == 'stats':
          stats_history.append(ev)
        elif kind == 'crash':
          crash_events.append(ev)
        elif kind == 'end':
          end_ev = ev

  log_path = os.path.join(arm_dir, 'redline.log')
  line_target_progress = ''
  inflight: list[tuple[str, str, str]] = []
  tail_buf: collections.deque[str] = collections.deque(maxlen=5)
  if os.path.exists(log_path):
    with open(log_path, encoding='utf-8', errors='replace') as f:
      for ln in f:
        tail_buf.append(ln.rstrip('\n'))
        m_line = RE_LOG_LINE_TARGET.search(ln)
        if m_line:
          line_target_progress = f'{m_line.group(1)}/{m_line.group(2)}'
        m_crash = RE_INFLIGHT_CRASH.search(ln)
        if m_crash:
          inflight.append(
              (m_crash.group(1), m_crash.group(2), m_crash.group(3)))
  log_tail = list(tail_buf)

  latest = stats_history[-1] if stats_history else {}
  elapsed = float(latest.get('t') or end_ev.get('t') or 0.0)
  hour_ref = None
  if len(stats_history) >= 2 and elapsed > 0:
    target_t = max(0.0, elapsed - 3600.0)
    hour_ref = min(
        stats_history[:-1],
        key=lambda e: abs(float(e.get('t', 0.0)) - target_t))

  execs = int(latest.get('executions', 0))
  rate_lifetime = (execs / elapsed) if elapsed > 0 else 0.0
  if hour_ref is not None:
    dt = elapsed - float(hour_ref.get('t', 0.0))
    dexec = execs - int(hour_ref.get('executions', 0))
    rate_1h = (dexec / dt) if dt > 0 else rate_lifetime
    dcov = (float(latest.get('coverage', 0.0)) -
            float(hour_ref.get('coverage', 0.0))) * 100.0
  else:
    rate_1h = rate_lifetime
    dcov = 0.0

  start_cfg = start_ev.get('config', {}) if isinstance(
      start_ev.get('config'), dict) else {}
  d8_path = meta.get('d8_path') or start_cfg.get('d8', '')
  d8_args = start_cfg.get('d8_args') or []
  if not isinstance(d8_args, list):
    d8_args = []

  targets_path = meta.get('targets_file') or os.path.join(
      arm_dir, 'targets.json')
  targets_meta = _read_json(targets_path) if targets_path else {}
  targets_stats = targets_meta.get('stats', {}) if isinstance(
      targets_meta.get('stats'), dict) else {}
  fallback_target_total = (
      int(targets_stats.get('line_targets', 0)) +
      int(targets_stats.get('function_targets', 0)) if targets_stats else None)
  fallback_focus_total = (
      int(targets_stats.get('focus_edges', 0))
      if targets_stats and 'focus_edges' in targets_stats else None)

  # Index .txt crash reports on disk to preserve stderr and match exact files.
  crashes_dir = os.path.join(arm_dir, 'crashes')
  reports_by_txt_path: dict[str, dict[str, str]] = {}
  reports_by_sig: dict[str, list[tuple[str, dict[str, str]]]] = {}
  for txt_path in sorted(glob.glob(os.path.join(crashes_dir, 'crash_*.txt'))):
    rep = _parse_report_txt(txt_path)
    reports_by_txt_path[os.path.abspath(txt_path)] = rep
    sig = rep.get('signature', '')
    if sig:
      reports_by_sig.setdefault(sig, []).append((txt_path, rep))

  if 'asan' in meta:
    is_asan = bool(meta['asan'])
  else:
    variant_str = str(meta.get('build_variant', ''))
    d8_parent = os.path.basename(os.path.dirname(d8_path))
    is_asan = _variant_has_asan(variant_str) or _variant_has_asan(d8_parent)
  redline_bin = meta.get('redline_bin') or 'out/rl_bin/redline'
  nodump_so = meta.get('nodump_so', '')

  # Collect all bugs by signature from events.jsonl, signatures.txt, and logs.
  bugs_by_sig: dict[str, dict] = {}
  for ev in crash_events:
    sig = ev.get('signature', '')
    if not sig:
      continue
    rep_list = reports_by_sig.get(sig, [])
    ev_path = ev.get('path') or ''
    rep_path = ''
    rep: dict[str, str] = {}
    if ev_path and ev_path.endswith('.js'):
      candidate_txt = os.path.abspath(ev_path[:-3] + '.txt')
      if candidate_txt in reports_by_txt_path:
        rep_path = candidate_txt
        rep = reports_by_txt_path[candidate_txt]
    if not rep and rep_list:
      rep_path, rep = rep_list[0]
    if not ev_path and rep_path:
      ev = dict(ev, path=rep_path[:-4] + '.js')
    classification = _classify_crash(ev, rep)
    new_count = int(ev.get('signature_count', 1))
    has_min_repro = bool(
        ev.get('repro') or ev.get('bundle_min') or ev.get('minimized') or
        rep.get('minimized', '').startswith('yes') or
        rep.get('repro') not in _UNSET_REPORT_VALS or
        rep.get('bundle_min') not in _UNSET_REPORT_VALS)
    existing = bugs_by_sig.get(sig)
    if existing:
      new_count = max(existing['count'], new_count)
      existing['count'] = new_count
      existing_rank = CLASSIFICATION_RANK.get(existing['classification'], 0)
      new_rank = CLASSIFICATION_RANK.get(classification, 0)
      if existing.get('saved') and not ev.get('saved'):
        continue
      if existing.get('saved') and new_rank < existing_rank:
        continue
      if (existing.get('saved') and new_rank == existing_rank and
          existing.get('has_min_repro') and not has_min_repro):
        continue
    cmd, artifacts = _build_repro_command(
        classification,
        d8_path,
        d8_args,
        ev,
        rep,
        crashes_dir,
        redline_bin,
        is_asan,
        nodump_so,
    )
    if rep_path and rep_path not in artifacts:
      artifacts.append(rep_path)

    if classification == 'state-dependent':
      has_bundle_min = bool(
          ev.get('bundle_min') or
          rep.get('bundle_min') not in _UNSET_REPORT_VALS)
      rate_str = (
          ev.get('bundle_min_rate')
          if has_bundle_min and ev.get('bundle_min_rate') else
          (f'{ev.get("bundle_hits", 0)}/{ev.get("bundle_runs", 0)}'
           if 'bundle_runs' in ev else
           (rep.get('bundle_min_rate') if has_bundle_min and
            rep.get('bundle_min_rate') else rep.get('bundle_rate', '?'))))
    elif classification == 'deterministic':
      rate_str = (
          ev.get('repro_rate') or rep.get('repro_rate') or
          f'{ev.get("reproduced", 0)}/{ev.get("reruns", 0)}')
    elif classification == 'amplified':
      rate_str = (
          ev.get('repro_rate') or rep.get('repro_rate') or
          (f'{ev.get("amplified_hits", 0)}/{ev.get("amplified_processes", 0)}'
           f' ({ev.get("amplified_args", "")})'))
    else:
      rate_str = f'0/{ev.get("reruns", 3)} (flaky)'

    bugs_by_sig[sig] = {
        'arm': name,
        'signature': sig,
        'signal': ev.get('signal', rep.get('signal', '?')),
        'classification': classification,
        'count': new_count,
        'saved': bool(ev.get('saved')),
        'has_min_repro': has_min_repro,
        'repro_rate': rate_str,
        'random_args': (ev.get('d8_random_args') or rep.get('d8_random_args') or
                        ''),
        'command': cmd,
        'artifacts': artifacts,
        'stderr': ev.get('stderr') or rep.get('_stderr_head', ''),
    }

  # Fallback: check signatures.txt for any signature missing from events.jsonl.
  sig_file = os.path.join(crashes_dir, 'signatures.txt')
  if os.path.exists(sig_file):
    with open(sig_file, encoding='utf-8', errors='replace') as f:
      for line in f:
        if not line.strip() or line.startswith('#'):
          continue
        parts = [p.strip() for p in line.rsplit(' | ', 6)]
        if len(parts) < 7:
          continue
        sig, cnt_s, _, _, _, _, first_file = parts[:7]
        cnt = int(cnt_s) if cnt_s.isdigit() else 1
        if sig in bugs_by_sig:
          bugs_by_sig[sig]['count'] = max(bugs_by_sig[sig]['count'], cnt)
          continue
        rep_path = ''
        rep = {}
        resolved_first = (
            first_file if not first_file or os.path.isabs(first_file) else
            os.path.join(crashes_dir, first_file))
        if resolved_first and resolved_first.endswith('.js'):
          candidate_txt = os.path.abspath(resolved_first[:-3] + '.txt')
          if candidate_txt in reports_by_txt_path:
            rep_path = candidate_txt
            rep = reports_by_txt_path[candidate_txt]
        if not rep:
          rep_list = reports_by_sig.get(sig, [])
          if rep_list:
            rep_path, rep = rep_list[0]
        if not resolved_first and rep_path:
          resolved_first = rep_path[:-4] + '.js'
        ev_stub = {'path': resolved_first}
        classification = _classify_crash(ev_stub, rep)
        cmd, artifacts = _build_repro_command(
            classification,
            d8_path,
            d8_args,
            ev_stub,
            rep,
            crashes_dir,
            redline_bin,
            is_asan,
            nodump_so,
        )
        if rep_path and rep_path not in artifacts:
          artifacts.append(rep_path)
        has_bundle_min = rep.get('bundle_min') not in _UNSET_REPORT_VALS
        fallback_rate = ((rep.get('bundle_min_rate') if has_bundle_min else '')
                         or rep.get('repro_rate') or rep.get('bundle_rate') or
                         rep.get('amplified_rate') or '?')
        bugs_by_sig[sig] = {
            'arm':
                name,
            'signature':
                sig,
            'signal':
                rep.get('signal', '?'),
            'classification':
                classification,
            'count':
                cnt,
            'saved':
                bool(resolved_first),
            'has_min_repro':
                bool(
                    rep.get('minimized', '').startswith('yes') or
                    rep.get('repro') not in _UNSET_REPORT_VALS or
                    has_bundle_min),
            'repro_rate':
                fallback_rate,
            'random_args':
                rep.get('d8_random_args', ''),
            'command':
                cmd,
            'artifacts':
                artifacts,
            'stderr':
                rep.get('_stderr_head', ''),
        }

  # Catch in-flight crashes currently mid-triage in redline.log.
  for worker_id, crash_num, sig in inflight:
    if sig not in bugs_by_sig:
      bugs_by_sig[sig] = {
          'arm':
              name,
          'signature':
              sig,
          'signal':
              '?',
          'classification':
              'in-flight (triaging)',
          'count':
              int(crash_num),
          'saved':
              False,
          'repro_rate':
              'pending minimization/triage',
          'random_args':
              '',
          'command':
              f'# Still triaging on worker {worker_id}. Check {crashes_dir}',
          'artifacts': [log_path],
          'stderr':
              '',
      }

  status = 'RUNNING' if alive else ('ENDED (' +
                                    end_ev.get('reason', 'unknown') +
                                    ')' if end_ev else 'STOPPED')

  alerts: list[str] = []
  if not alive and not end_ev:
    alerts.append(
        f'{name}: process dead without clean end event (see {log_path})')
  elif not alive and end_ev.get('reason') == 'failure':
    alerts.append(f'{name}: arm ended with failure (see {log_path})')
  if meta.get('change_based') and elapsed > 900 and int(
      latest.get('target_total', 0)) > 0 and int(latest.get('target_found',
                                                            0)) == 0:
    alerts.append(
        f'{name}: 0/{latest.get("target_total")} CL target edges reached '
        f'after {_format_duration(elapsed)}. Check CL flags or tier focus.')
  if int(import_done_ev.get('failed', 0)) > 0 and int(
      import_done_ev.get('added', 0)) == 0:
    alerts.append(f'{name}: all imported tests failed during startup')

  total_out = (
      int(latest.get('succeeded', 0)) + int(latest.get('failed', 0)) +
      int(latest.get('timed_out', 0)) + int(latest.get('crashed', 0)))
  tmo_pct = (100.0 * int(latest.get('timed_out', 0)) /
             total_out if total_out else 0.0)
  ok_pct = (100.0 * int(latest.get('succeeded', 0)) /
            total_out if total_out else 0.0)
  fuzz_execs = int(latest.get('fuzz_executions', 0))
  invalid_cnt = int(latest.get('invalid', 0))
  produced = fuzz_execs + invalid_cnt
  inv_pct = (100.0 * invalid_cnt / produced if produced else 0.0)
  if total_out > 100 and tmo_pct > 15.0:
    alerts.append(f'{name}: high timeout rate ({tmo_pct:.1f}%)')

  duration_sec = int(
      meta.get('duration_sec') or start_cfg.get('duration_sec') or 0)

  return {
      'name': name,
      'arm_dir': arm_dir,
      'status': status,
      'pid': pid,
      'meta': meta,
      'd8_path': d8_path,
      'd8_args': d8_args,
      'jobs': meta.get('jobs') or start_cfg.get('jobs', '?'),
      'duration_sec': duration_sec,
      'timeout_ms': meta.get('timeout_ms', '?'),
      'elapsed_sec': elapsed,
      'elapsed_str': _format_duration(elapsed),
      'execs': execs,
      'rate_lifetime': rate_lifetime,
      'rate_1h': rate_1h,
      'cov_pct': float(latest.get('coverage', 0.0)) * 100.0,
      'cov_delta_1h': dcov,
      'found_edges': int(latest.get('found_edges', 0)),
      'num_edges': int(latest.get('num_edges', start_ev.get('num_edges', 0))),
      'corpus': int(latest.get('corpus', 0)),
      'target_found': latest.get('target_found'),
      'target_total':
          (latest.get('target_total') if latest.get('target_total') is not None
           else start_ev.get('target_edges', fallback_target_total)),
      'line_target_progress': line_target_progress,
      'focus_found': latest.get('focus_found'),
      'focus_total':
          (latest.get('focus_total') if latest.get('focus_total') is not None
           else start_ev.get('focus_edges', fallback_focus_total)),
      'ok_pct': ok_pct,
      'tmo_pct': tmo_pct,
      'inv_pct': inv_pct,
      'total_crashes': int(latest.get('crashed', 0)),
      'bugs': list(bugs_by_sig.values()),
      'alerts': alerts,
      'log_tail': log_tail,
  }


def find_arm_dirs(run_root: str) -> list[str]:
  arms_sub = os.path.join(run_root, 'arms')
  base = arms_sub if os.path.isdir(arms_sub) else run_root
  if os.path.exists(os.path.join(base, 'events.jsonl')) or os.path.exists(
      os.path.join(base, 'arm_meta.json')):
    return [base]
  dirs = []
  for entry in sorted(os.listdir(base)):
    full = os.path.join(base, entry)
    if os.path.isdir(full) and (
        os.path.exists(os.path.join(full, 'events.jsonl')) or
        os.path.exists(os.path.join(full, 'arm_meta.json')) or
        os.path.exists(os.path.join(full, 'redline.log'))):
      dirs.append(full)
  return dirs


def _format_duration_cap(duration_sec: int) -> str:
  if duration_sec <= 0:
    return 'until stopped'
  if duration_sec % 3600 == 0:
    return f'{duration_sec // 3600} h'
  return _format_duration(duration_sec)


def render_markdown(arms: list[dict], mode: str, host: str = '') -> str:
  lines: list[str] = []
  is_remote = bool(host and host != 'local')
  if mode == 'immediate':
    lines.append('### Active Fuzzing Arms')
    lines.append(
        '| Arm | Status | Build | Scope | V8 Tier Focus | Workers | Duration | '
        'Timeout | Effective d8 Flags | Initial Tests | Output Dir |')
    lines.append('|---|---|---|---|---|---|---|---|---|---|---|')
    for a in arms:
      m = a['meta']
      build = m.get('build_variant', '?')
      if m.get('builtins_coverage') is False:
        build += ' (no-builtins-cov)'
      if m.get('change_based'):
        scope = 'CL-targeted'
        if a.get('target_total'):
          scope += f' ({a["target_total"]} edges)'
      else:
        scope = 'Full V8'
      preset = str(m.get('preset', '?'))
      focus = PRESET_V8_LABELS.get(preset, preset)
      eff_flags = ' '.join(a['d8_args']) or ' '.join(m.get('extra_flags') or [])
      dur = _format_duration_cap(int(a.get('duration_sec', 0) or 0))
      lines.append(
          f'| {_md_cell(a["name"])} | {_md_cell(a["status"])} (PID {a["pid"]}) | '
          f'`{_md_cell(build)}` | {_md_cell(scope)} | '
          f'{_md_cell(focus)} | {a["jobs"]} | {dur} | '
          f'{a["timeout_ms"]} ms | `{_md_cell(eff_flags)}` | '
          f'`{_md_cell(m.get("seeds", "?"))}` | `{_md_cell(a["arm_dir"])}` |')
    imm_alerts = [al for a in arms for al in a['alerts']]
    if imm_alerts:
      lines.append('\n### Alerts')
      for al in imm_alerts:
        lines.append(f'- WARNING: {al}')
    return '\n'.join(lines)

  lines.append('### Hourly Fuzzing Status')
  lines.append('| Arm | Status | Uptime | Runs (1h Rate) | Coverage (1h Δ) | '
               'Saved Inputs | CL Code Hit | Pass / Timeout / Syntax Err | '
               'Bug Sigs (Crashes) |')
  lines.append('|---|---|---|---|---|---|---|---|---|')
  all_bugs: list[dict] = []
  all_alerts: list[str] = []
  for a in arms:
    cov = f'{a["cov_pct"]:.2f}% ({a["found_edges"]}/{a["num_edges"]})'
    if a['cov_delta_1h']:
      cov += f' (+{a["cov_delta_1h"]:.2f}pp)'
    if a['target_total'] is not None and int(a['target_total']) > 0:
      tf = 0 if a['target_found'] is None else a['target_found']
      tgt = f'{tf}/{a["target_total"]}'
      if a['line_target_progress']:
        tgt += f' (lines {a["line_target_progress"]})'
      if a['focus_total'] is not None:
        ff = 0 if a['focus_found'] is None else a['focus_found']
        tgt += f', focus {ff}/{a["focus_total"]}'
    else:
      tgt = 'Full V8'
    health = f'{a["ok_pct"]:.1f}% / {a["tmo_pct"]:.1f}% / {a["inv_pct"]:.1f}%'
    sig_cnt = len(a['bugs'])
    crash_sum = sum(b['count'] for b in a['bugs']) or a['total_crashes']
    lines.append(f'| {_md_cell(a["name"])} | {_md_cell(a["status"])} | '
                 f'{_md_cell(a["elapsed_str"])} | '
                 f'{a["execs"]:,} ({a["rate_1h"]:.0f}/s) | {_md_cell(cov)} | '
                 f'{a["corpus"]:,} | {_md_cell(tgt)} | {health} | '
                 f'{sig_cnt} ({crash_sum}) |')
    all_bugs.extend(a['bugs'])
    all_alerts.extend(a['alerts'])

  lines.append(
      '\n### All Bugs Found (Deterministic, State-Dependent, Amplified, Flaky, and In-Flight)'
  )
  if not all_bugs:
    lines.append('No crashes found so far.')
  else:
    lines.append(
        '| # | Arm | Type | Hits | Repro Rate | Signature | Repro Command & Artifacts |'
    )
    lines.append('|---|---|---|---|---|---|---|')
    for idx, b in enumerate(all_bugs, 1):
      cmd_cell = f'`{_md_cell(b["command"])}`'
      if b['artifacts']:
        cmd_cell += ' (' + ', '.join(
            f'`{_md_cell(p)}`' for p in b['artifacts'][:2]) + ')'
      lines.append(
          f'| {idx} | {_md_cell(b["arm"])} | '
          f'**{_md_cell(b["classification"])}** | {b["count"]} | '
          f'`{_md_cell(b["repro_rate"])}` | `{_md_cell(b["signature"])}` | '
          f'{cmd_cell} |')

    lines.append('\n#### Detailed Crash Traces')
    run_where = f' (run on `{host}`)' if is_remote else ''
    for idx, b in enumerate(all_bugs, 1):
      lines.append(
          f'- **#{idx} [{b["classification"]}] `{b["signature"]}`** ({b["arm"]})'
      )
      if b['random_args']:
        lines.append(
            f'  - Random flags on crashing worker: `{b["random_args"]}`')
      lines.append(f'  - Reproduce{run_where}: `{b["command"]}`')
      if '--bundle-reprl' in b['command']:
        lines.append(
            '  - Note: `--bundle-reprl` replays the short script sequence in '
            'one `d8` process so the crash reproduces.')
      if (is_remote and b['artifacts'] and
          b['classification'] != 'in-flight (triaging)'):
        lines.append(
            f'  - Copy testcase locally: `scp {host}:{b["artifacts"][0]} .`')
      if b['stderr']:
        first_err = ' | '.join(b['stderr'].splitlines()[:3])
        lines.append(f'  - Stderr: `{first_err}`')

  if all_alerts:
    lines.append('\n### Alerts')
    for al in all_alerts:
      lines.append(f'- WARNING: {al}')
  return '\n'.join(lines)


def main(argv: list[str]) -> int:
  parser = argparse.ArgumentParser(
      description='Report Redline arm setup, coverage, and all crashes.')
  parser.add_argument('run_root', help='Run root directory or arm directory')
  parser.add_argument(
      '--mode',
      choices=('immediate', 'hourly'),
      default='hourly',
      help='Report format')
  parser.add_argument(
      '--host',
      default='',
      help='Remote Cloudtop hostname to show in reproducer instructions')
  parser.add_argument('--json', action='store_true', help='Output raw JSON')
  args = parser.parse_args(argv)

  if not os.path.exists(args.run_root):
    print(f'Error: {args.run_root} does not exist', file=sys.stderr)
    return 1
  arm_dirs = find_arm_dirs(args.run_root)
  if not arm_dirs:
    print(
        f'Error: no arm directories found under {args.run_root}',
        file=sys.stderr)
    return 1
  arms = [collect_arm(d) for d in arm_dirs]
  if args.json:
    print(json.dumps(arms, indent=2))
  else:
    print(render_markdown(arms, args.mode, host=args.host))
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
