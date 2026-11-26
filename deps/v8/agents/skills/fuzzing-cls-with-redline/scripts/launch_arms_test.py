#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for launch_arms.py."""

import json
import os
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import launch_arms  # pylint: disable=wrong-import-position


class LaunchArmsTest(unittest.TestCase):

  def test_build_fuzz_command_import_and_generative_and_asan(self):
    with tempfile.TemporaryDirectory() as tmp:
      os.makedirs(os.path.join(tmp, 'test', 'mjsunit', 'wasm'))
      nodump = os.path.join(tmp, 'nodump.so')
      with open(nodump, 'w', encoding='utf-8') as f:
        f.write('')

      arm_import = {
          'name': 'arm-asan-slow',
          'build_variant': 'asan_slow_dcheck',
          'builtins_coverage': True,
          'preset': 'wasm',
          'd8_extra_args': ['--stress-wasm-memory-moving', '--single-threaded'],
          'generator_weights': {
              'TypedArrayDecl': 3,
              'WasmModule': 6
          },
          'randomize_d8_flags': True,
          'd8_flag_profile': 'fuzzilli',
          'jobs': 8,
          'duration_sec': 43200,
          'timeout_ms': 800,
          'seeds': 'test/mjsunit/wasm,test/mjsunit/nonexistent',
      }
      cmd, meta = launch_arms.build_fuzz_command(
          arm_import,
          v8_dir=tmp,
          arm_dir=os.path.join(tmp, 'arms', 'arm-asan-slow'),
          d8_path='/v8/out/rl_asan_slow/d8',
          redline_bin='/v8/out/rl_bin/redline',
          nodump_so=nodump,
          targets_file='',
      )
      self.assertIn('--import=test/mjsunit/wasm', cmd)
      self.assertNotIn('nonexistent', ' '.join(cmd))
      self.assertIn('--duration=43200', cmd)
      self.assertIn('--d8-flag-profile=fuzzilli', cmd)
      self.assertIn('--generator-weights=TypedArrayDecl=3,WasmModule=6', cmd)
      self.assertIn(
          '--d8-extra-args=--stress-wasm-memory-moving --single-threaded', cmd)
      self.assertTrue(any('ASAN_OPTIONS=abort_on_error=1' in c for c in cmd))
      self.assertTrue(any(f'LD_PRELOAD={nodump}' in c for c in cmd))
      self.assertTrue(meta['asan'])
      self.assertEqual(meta['duration_sec'], 43200)
      self.assertIsInstance(meta['extra_flags'], list)
      self.assertIs(meta['builtins_coverage'], True)

      arm_gen = {
          'name': 'arm-generative',
          'build_variant': 'slow_dcheck',
          'builtins_coverage': False,
          'preset': 'default',
          'd8_extra_args': [],
          'generator_weights': {},
          'randomize_d8_flags': False,
          'jobs': 6,
          'timeout_ms': 700,
          'seeds': 'generative',
      }
      cmd_gen, meta_gen = launch_arms.build_fuzz_command(
          arm_gen,
          v8_dir=tmp,
          arm_dir=os.path.join(tmp, 'arms', 'arm-generative'),
          d8_path='/v8/out/rl_slow_dcheck/d8',
          redline_bin='/v8/out/rl_bin/redline',
          nodump_so=nodump,
          targets_file='/run/targets.json',
      )
      self.assertIn('--prelude=none', cmd_gen)
      self.assertFalse(any(c.startswith('--import=') for c in cmd_gen))
      self.assertIn('--no-randomize-d8-flags', cmd_gen)
      self.assertIn('--targets=/run/targets.json', cmd_gen)
      self.assertIn('--d8-extra-args=--no-fuzzilli-enable-builtins-coverage',
                    cmd_gen)
      self.assertIs(meta_gen['builtins_coverage'], False)
      self.assertEqual(meta_gen['seeds'], 'generative')

  def test_schema_validation_rejects_extra_flags_and_bad_profile(self):
    with self.assertRaises(ValueError):
      launch_arms.validate_arm_schema({
          'name': 'arm-bad',
          'extra_flags': ['--future'],
      })
    with self.assertRaises(ValueError):
      launch_arms.validate_arm_schema({
          'name': 'arm-both',
          'd8_extra_args': ['--maglev'],
          'extra_flags': ['--future'],
      })
    with self.assertRaises(ValueError):
      launch_arms.validate_arm_schema({
          'name': '../escape',
          'd8_extra_args': [],
      })
    with self.assertRaises(ValueError):
      launch_arms.validate_arm_schema({
          'name': 'arm-missing-variant',
          'd8_extra_args': [],
      })
    with self.assertRaises(ValueError):
      launch_arms.validate_arm_schema({
          'name': 'arm-bad-excluded',
          'build_variant': 'slow_dcheck',
          'd8_extra_args': [],
          'excluded_flags': '--not-a-list',
      })
    with self.assertRaises(ValueError):
      launch_arms.validate_arm_schema({
          'name': 'arm-prof',
          'build_variant': 'slow_dcheck',
          'd8_extra_args': [],
          'randomize_d8_flags': True,
          'd8_flag_profile': 'unknown',
      })

  def test_dry_run_reads_plan_json(self):
    with tempfile.TemporaryDirectory() as tmp:
      plan_path = os.path.join(tmp, 'plan.json')
      with open(plan_path, 'w', encoding='utf-8') as f:
        json.dump(
            {
                'arms': [{
                    'name': 'arm-asan-slow',
                    'build_variant': 'asan_slow_dcheck',
                    'preset': 'default',
                    'd8_extra_args': ['--single-threaded'],
                    'generator_weights': {},
                    'randomize_d8_flags': True,
                    'd8_flag_profile': 'fuzzilli',
                    'jobs': 4,
                    'timeout_ms': 800,
                    'seeds': ['generative'],
                }]
            }, f)
      res = launch_arms.launch(tmp, os.path.join(tmp, 'run'), plan_path,
                               'origin/main', 'HEAD', True)
      self.assertEqual(len(res), 1)
      self.assertEqual(res[0]['name'], 'arm-asan-slow')

  def test_compute_targets_and_launch_non_dry_run_with_mocks(self):
    with tempfile.TemporaryDirectory() as tmp:
      v8_dir = os.path.join(tmp, 'v8')
      run_root = os.path.join(tmp, 'run')
      tools_dir = os.path.join(v8_dir, 'experimental', 'redline', 'tools')
      os.makedirs(tools_dir)
      os.makedirs(os.path.join(v8_dir, 'out', 'rl_bin'))
      os.makedirs(os.path.join(v8_dir, 'out', 'rl_asan'))
      redline_bin = os.path.join(v8_dir, 'out', 'rl_bin', 'redline')
      d8_bin = os.path.join(v8_dir, 'out', 'rl_asan', 'd8')
      for p in (redline_bin, d8_bin):
        with open(p, 'w', encoding='utf-8') as f:
          f.write('#!/bin/sh\nexit 0\n')
      fake_ct = os.path.join(tools_dir, 'change_targets.py')
      with open(fake_ct, 'w', encoding='utf-8') as f:
        f.write(
            'import argparse, json\n'
            'p = argparse.ArgumentParser()\n'
            'p.add_argument("--d8")\n'
            'p.add_argument("--repo")\n'
            'p.add_argument("--base")\n'
            'p.add_argument("--head")\n'
            'p.add_argument("--out")\n'
            'p.add_argument("--edge-map", default="")\n'
            'a = p.parse_args()\n'
            'with open(a.out, "w") as out:\n'
            '  json.dump({"stats": {"line_targets": 4, '
            '"function_targets": 1}, '
            '"flags": ["--turbolev", "--skip-me", "--negated-flag"]}, out)\n')

      plan_path = os.path.join(tmp, 'plan.json')
      with open(plan_path, 'w', encoding='utf-8') as f:
        json.dump(
            {
                'arms': [{
                    'name': 'arm-cb',
                    'build_variant': 'asan',
                    'change_based': True,
                    'preset': 'turbolev',
                    'd8_extra_args': ['--maglev', '--no-negated-flag'],
                    'excluded_flags': ['--skip-me'],
                    'jobs': 2,
                    'timeout_ms': 600,
                    'seeds': 'generative',
                }]
            }, f)

      real_popen = launch_arms.subprocess.Popen
      fake_proc = mock.MagicMock(pid=424242)
      fake_proc.poll.return_value = None
      popen_calls: list[list[str]] = []

      def _popen_side_effect(cmd, *args, **kwargs):
        if cmd and cmd[0] == redline_bin:
          popen_calls.append(list(cmd))
          return fake_proc
        return real_popen(cmd, *args, **kwargs)

      with mock.patch.object(
          launch_arms.subprocess, 'Popen', side_effect=_popen_side_effect):
        launched = launch_arms.launch(
            v8_dir,
            run_root,
            plan_path,
            'origin/main',
            'HEAD',
            False,
            startup_wait_sec=0.0,
        )

      self.assertEqual(len(launched), 1)
      self.assertEqual(launched[0]['pid'], 424242)
      self.assertEqual(len(popen_calls), 1)
      argv = popen_calls[0]
      self.assertIn('--targets=' + os.path.join(run_root, 'targets_asan.json'),
                    argv)
      self.assertIn('--d8-extra-args=--maglev --no-negated-flag --turbolev',
                    argv)
      self.assertNotIn('--skip-me', ' '.join(argv))
      self.assertNotIn('--negated-flag', ' '.join(argv))
      arm_dir = os.path.join(run_root, 'arms', 'arm-cb')
      with open(os.path.join(arm_dir, 'arm_meta.json'), encoding='utf-8') as f:
        meta = json.load(f)
      self.assertTrue(meta['change_based'])
      with open(os.path.join(arm_dir, 'redline.pid'), encoding='utf-8') as f:
        self.assertEqual(f.read().strip(), '424242')

      # Relaunching while arm-cb is alive must fail unless --only-stopped.
      with mock.patch.object(
          launch_arms, '_pid_alive_redline', return_value=(True, 424242)):
        with self.assertRaises(RuntimeError):
          launch_arms.launch(
              v8_dir,
              run_root,
              plan_path,
              'origin/main',
              'HEAD',
              False,
              startup_wait_sec=0.0,
          )
        skipped = launch_arms.launch(
            v8_dir,
            run_root,
            plan_path,
            'origin/main',
            'HEAD',
            False,
            only_stopped=True,
            startup_wait_sec=0.0,
        )
        self.assertEqual(skipped, [])


if __name__ == '__main__':
  unittest.main()
