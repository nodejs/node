#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for prepare_run.py."""

import json
import os
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import prepare_run  # pylint: disable=wrong-import-position


class PrepareRunTest(unittest.TestCase):

  def test_gn_args_and_collect_variant_specs(self):
    plan = {
        'arms': [
            {
                'name': 'arm-asan-slow',
                'build_variant': 'asan_slow_dcheck',
                'builtins_coverage': True,
                'change_based': False,
                'extra_gn_args': ['v8_enable_memory_corruption_api=true'],
            },
            {
                'name': 'arm-change-counters',
                'build_variant': 'counters_slow_dcheck',
                'builtins_coverage': False,
                'change_based': True,
                'extra_gn_args': [],
            },
            {
                'name': 'arm-arm64sim',
                'build_variant': 'arm64sim',
                'builtins_coverage': True,
                'change_based': True,
                'extra_gn_args': [],
            },
        ]
    }
    specs = prepare_run.collect_variant_specs(plan)
    by_var = {s['variant']: s for s in specs}
    self.assertEqual(by_var['asan_slow_dcheck']['out_dir'], 'rl_asan_slow')
    self.assertEqual(by_var['counters_slow_dcheck']['out_dir'], 'rl_counters')

    asan_args = prepare_run.gn_args_for_variant(
        'asan_slow_dcheck',
        builtins_coverage=True,
        extra_gn_args=['v8_enable_memory_corruption_api=true'],
        use_remoteexec=True,
    )
    self.assertIn('is_asan=true', asan_args)
    self.assertIn('v8_enable_builtins_profiling=true', asan_args)
    self.assertIn('v8_enable_memory_corruption_api=true', asan_args)
    self.assertIn('use_remoteexec=true', asan_args)

    arm64_args = prepare_run.gn_args_for_variant(
        'arm64sim',
        builtins_coverage=False,
        change_based=True,
        use_remoteexec=False,
    )
    self.assertIn('v8_target_cpu="arm64"', arm64_args)
    self.assertIn('sanitizer_coverage_flags="inline-8bit-counters,pc-table"',
                  arm64_args)
    self.assertIn('v8_enable_builtins_profiling=false', arm64_args)
    self.assertNotIn('use_remoteexec=true', arm64_args)

  def test_stage_maps_variant_out_dirs_and_runs_selftest(self):
    with tempfile.TemporaryDirectory() as tmp:
      v8_dir = os.path.join(tmp, 'v8')
      run_root = os.path.join(tmp, 'run')
      # Create out/rl_bin, out/rl_asan_slow, out/rl_counters, and redline data.
      for sub in ('out/rl_bin', 'out/rl_asan_slow', 'out/rl_counters',
                  'experimental/redline/presets',
                  'experimental/redline/testdata'):
        os.makedirs(os.path.join(v8_dir, sub), exist_ok=True)
      for rel in (
          'out/rl_bin/redline',
          'out/rl_bin/snapshot_blob.bin',
          'out/rl_bin/icudtl.dat',
          'out/rl_asan_slow/d8',
          'out/rl_asan_slow/snapshot_blob.bin',
          'out/rl_asan_slow/icudtl.dat',
          'out/rl_counters/d8',
          'out/rl_counters/snapshot_blob.bin',
          'out/rl_counters/icudtl.dat',
          'experimental/redline/presets/default.json',
          'experimental/redline/testdata/sample.txt',
      ):
        with open(os.path.join(v8_dir, rel), 'w', encoding='utf-8') as f:
          f.write('stub')

      plan_path = os.path.join(tmp, 'plan.json')
      with open(plan_path, 'w', encoding='utf-8') as f:
        json.dump(
            {
                'arms': [
                    {
                        'name': 'arm-asan-slow',
                        'build_variant': 'asan_slow_dcheck',
                    },
                    {
                        'name': 'arm-change-counters',
                        'build_variant': 'counters_slow_dcheck',
                    },
                ]
            }, f)

      run_calls: list[list[str]] = []

      def _fake_run(cmd, *args, **kwargs):
        run_calls.append(list(cmd))
        # Simulate clang producing nodump.so.
        if '-o' in cmd:
          out_so = cmd[cmd.index('-o') + 1]
          with open(out_so, 'w', encoding='utf-8') as sf:
            sf.write('so')
        return mock.MagicMock(returncode=0, stdout='PASS all\n', stderr='')

      with mock.patch.object(
          prepare_run.subprocess, 'run',
          side_effect=_fake_run), mock.patch.object(
              prepare_run.shutil, 'which', return_value='/usr/bin/clang'):
        res = prepare_run.stage_and_selftest(v8_dir, run_root, plan_path)

      self.assertTrue(
          os.path.exists(
              os.path.join(run_root, 'bin', 'asan_slow_dcheck', 'd8')))
      self.assertTrue(
          os.path.exists(
              os.path.join(run_root, 'bin', 'counters_slow_dcheck', 'd8')))
      self.assertTrue(
          os.path.exists(
              os.path.join(run_root, 'bin', 'redline_bundle', 'presets',
                           'default.json')))
      self.assertEqual(len(res['selftest']), 3)
      asan_selftest = next(
          c for c in run_calls
          if 'selftest' in c and any('asan_slow_dcheck' in a for a in c))
      self.assertTrue(any('ASAN_OPTIONS=' in a for a in asan_selftest))

  def test_selftest_tolerates_only_asan_amplified_flake(self):
    self.assertTrue(
        prepare_run._selftest_passes(
            'PASS reprl\nFAIL amplified crashes: timed out\n', 1, is_asan=True))
    self.assertFalse(
        prepare_run._selftest_passes(
            'PASS reprl\nFAIL amplified crashes: timed out\n', 1,
            is_asan=False))
    self.assertFalse(
        prepare_run._selftest_passes(
            'FAIL reprl handshake\nFAIL amplified crashes\n', 1, is_asan=True))

  def test_reclient_fallback_does_not_mask_compile_error(self):
    calls: list[list[str]] = []

    def _reclient_then_ok(cmd, *args, **kwargs):
      calls.append(list(cmd))
      if cmd[0] == 'gn' and any('use_remoteexec=true' in a for a in cmd):
        return mock.MagicMock(
            returncode=1, stdout='', stderr='reproxy not authenticated')
      return mock.MagicMock(returncode=0, stdout='', stderr='')

    with mock.patch.object(
        prepare_run.subprocess, 'run', side_effect=_reclient_then_ok):
      prepare_run._run_gn_and_ninja('/v8', 'rl_bin', 'redline',
                                    ['is_debug=false'], True, {})
    self.assertEqual(len(calls), 3)
    self.assertIn('-j', calls[2])

    # Real compiler error must fail fast without retrying locally.
    compile_calls: list[list[str]] = []

    def _compile_err(cmd, *args, **kwargs):
      compile_calls.append(list(cmd))
      if cmd[0] == 'autoninja':
        return mock.MagicMock(
            returncode=1,
            stdout='',
            stderr='src/maglev/foo.cc:10: error: undeclared identifier')
      return mock.MagicMock(returncode=0, stdout='', stderr='')

    with mock.patch.object(
        prepare_run.subprocess, 'run', side_effect=_compile_err):
      with self.assertRaises(RuntimeError):
        prepare_run._run_gn_and_ninja('/v8', 'rl_bin', 'redline',
                                      ['is_debug=false'], True, {})
    self.assertEqual(len(compile_calls), 2)

  def test_probe_host_without_loginctl_and_has_end_event_and_reclient_re(self):
    with mock.patch.object(prepare_run.shutil, 'which', return_value=None):
      info = prepare_run.probe_host()
    self.assertEqual(info['linger'], 'unknown')
    self.assertIn('is_cloudtop', info)

    # _has_end_event must reflect the latest session after --only-stopped.
    with tempfile.TemporaryDirectory() as tmp:
      ev_path = os.path.join(tmp, 'events.jsonl')
      with open(ev_path, 'w', encoding='utf-8') as f:
        f.write('{"event":"start"}\n{"event":"end","reason":"duration"}\n')
      self.assertTrue(prepare_run._has_end_event(tmp))
      with open(ev_path, 'a', encoding='utf-8') as f:
        f.write('{"event":"start"}\n')
      self.assertFalse(prepare_run._has_end_event(tmp))
      with open(ev_path, 'a', encoding='utf-8') as f:
        f.write('{"event":"end","reason":"signal"}\n')
      self.assertTrue(prepare_run._has_end_event(tmp))

    # Word-bounded rbe avoids matching C++ identifiers containing "rbe".
    self.assertIsNone(
        prepare_run.RE_RECLIENT_ERROR.search('error: undeclared var verbena'))
    self.assertIsNotNone(
        prepare_run.RE_RECLIENT_ERROR.search('RBE auth failed'))
    self.assertIsNotNone(
        prepare_run.RE_RECLIENT_ERROR.search('reapi credhelper failed'))

    with self.assertRaises(ValueError):
      prepare_run.collect_variant_specs({'arms': [{'name': 'arm-no-variant'}]})

  def test_stop_run_ignores_recycled_pid_and_signals_redline(self):
    with tempfile.TemporaryDirectory() as tmp:
      arm_rl = os.path.join(tmp, 'arms', 'arm-redline')
      arm_other = os.path.join(tmp, 'arms', 'arm-recycled')
      os.makedirs(arm_rl)
      os.makedirs(arm_other)
      with open(
          os.path.join(arm_rl, 'redline.pid'), 'w', encoding='utf-8') as f:
        f.write('11111\n')
      with open(
          os.path.join(arm_other, 'redline.pid'), 'w', encoding='utf-8') as f:
        f.write('22222\n')

      killed_calls: list[tuple[int, int]] = []

      def _fake_cmdline(pid: int) -> str:
        if pid == 11111:
          return '/run/bin/redline_bundle/redline fuzz --out=arm-redline'
        return '/usr/bin/sleep 999'

      with mock.patch.object(
          prepare_run, '_pid_alive', return_value=True), mock.patch.object(
              prepare_run, '_read_cmdline',
              side_effect=_fake_cmdline), mock.patch.object(
                  prepare_run, '_find_scoped_pids',
                  return_value=[]), mock.patch.object(
                      prepare_run.os,
                      'kill',
                      side_effect=lambda p, s: killed_calls.append((p, s))):
        res = prepare_run.stop_run(tmp, timeout_sec=0.0)

      self.assertEqual(killed_calls, [(11111, prepare_run.signal.SIGINT)])
      self.assertEqual([a['arm'] for a in res['signaled']], ['arm-redline'])


if __name__ == '__main__':
  unittest.main()
