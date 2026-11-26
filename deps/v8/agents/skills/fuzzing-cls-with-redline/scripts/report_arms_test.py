#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for report_arms.py."""

import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import report_arms  # pylint: disable=wrong-import-position


class ReportArmsTest(unittest.TestCase):

  def _write(self, path: str, content: str) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8') as f:
      f.write(content)

  def test_reports_all_crash_classes_including_flaky_and_state_dependent(self):
    with tempfile.TemporaryDirectory() as tmp:
      arm1 = os.path.join(tmp, 'arms', 'arm-asan-slow')
      crashes1 = os.path.join(arm1, 'crashes')
      self._write(
          os.path.join(arm1, 'arm_meta.json'),
          json.dumps({
              'name': 'arm-asan-slow',
              'build_variant': 'asan_slow_dcheck',
              'change_based': False,
              'preset': 'maglev',
              'extra_flags': ['--maglev-assert'],
              'jobs': 10,
              'duration_sec': 43200,
              'timeout_ms': 800,
              'seeds': 'test/mjsunit/maglev',
              'd8_path': '/v8/out/rl_asan/d8',
              'nodump_so': '/run/nodump.so',
          }),
      )
      bundle_file = os.path.join(crashes1, 'crash_sig6_aaaa.bundle.min.js')
      self._write(
          bundle_file,
          '// redline state-dependent crash\n'
          '// signature: fatal maglev.cc:12 Check failed\n'
          '// run: /v8/out/rl_asan/d8 --maglev --bundle-reprl '
          'crash_sig6_aaaa.bundle.min.js\n',
      )
      repro_file = os.path.join(crashes1, 'crash_sig11_bbbb.repro.js')
      self._write(
          repro_file,
          '// signature: sig11 Foo\n'
          '// run: out/rl_asan/d8 --maglev crash_sig11_bbbb.repro.js\n',
      )
      flaky_file = os.path.join(crashes1, 'crash_sig6_cccc.js')
      self._write(flaky_file, 'function f() {}\n')
      self._write(
          os.path.join(crashes1, 'crash_sig6_cccc.txt'),
          'signal: 6\nreproducible: no\nstate_dependent: no\n'
          'd8_random_args: --stress-ic\n'
          '--- stderr ---\n# Fatal error in src/foo.cc, line 99\n',
      )
      events = [
          {
              't': 0.1,
              'event': 'start',
              'num_edges': 100000,
              'target_edges': 0,
              'config': {
                  'd8': '/v8/out/rl_asan/d8',
                  'd8_args': ['--maglev', '--no-turbofan', '--maglev-assert']
              },
          },
          {
              't': 1.0,
              'event': 'import_done',
              'total': 5,
              'added': 0,
              'redundant': 0,
              'failed': 5,
          },
          {
              't': 3600.0,
              'event': 'stats',
              'executions': 120000,
              'fuzz_executions': 100,
              'succeeded': 90000,
              'failed': 9000,
              'timed_out': 1000,
              'crashed': 4,
              'invalid': 100,
              'corpus': 1500,
              'coverage': 0.25,
              'found_edges': 25000,
              'num_edges': 100000,
          },
          # Deterministic crash with relative // run: path.
          {
              't': 100.0,
              'event': 'crash',
              'signature': 'sig11 Foo',
              'signature_count': 1,
              'signal': 11,
              'saved': True,
              'reproducible': True,
              'reproduced': 2,
              'reruns': 2,
              'repro_rate': '5/5',
              'repro': repro_file,
              'path': os.path.join(crashes1, 'crash_sig11_bbbb.js'),
              'stderr': 'SEGV in Foo',
          },
          # State-dependent crash -> must use --bundle-reprl.
          {
              't': 200.0,
              'event': 'crash',
              'signature': 'fatal maglev.cc:12 Check failed',
              'signature_count': 2,
              'signal': 6,
              'saved': True,
              'reproducible': False,
              'state_dependent': True,
              'bundle': os.path.join(crashes1, 'crash_sig6_aaaa.bundle.js'),
              'bundle_min': bundle_file,
              'bundle_min_rate': '3/3',
              'path': os.path.join(crashes1, 'crash_sig6_aaaa.js'),
              'stderr': 'Check failed',
          },
          # Flaky crash -> must still be reported with rc > 128 check even when
          # Redline includes candidate amplified_args alongside amplify: "no".
          {
              't': 300.0,
              'event': 'crash',
              'signature': 'fatal foo.cc:99 Check failed',
              'signature_count': 1,
              'signal': 6,
              'saved': True,
              'reproducible': False,
              'state_dependent': False,
              'amplify': 'no',
              'amplified_args': '--stress-runs=100',
              'reruns': 3,
              'd8_random_args': '--stress-ic',
              'path': flaky_file,
              'stderr': 'Fatal error in src/foo.cc, line 99',
          },
      ]
      self._write(
          os.path.join(arm1, 'events.jsonl'),
          '\n'.join(json.dumps(e) for e in events) + '\n',
      )
      self._write(
          os.path.join(arm1, 'redline.log'),
          '[12:00:00 I] [fuzz] worker 1: crash #1 of signature '
          '"fatal bar.cc:7 InFlight \\"quote\\"" (new crash edges, saving)\n',
      )

      arm = report_arms.collect_arm(arm1)
      # Invalid rate uses invalid / (fuzz_executions + invalid) = 50.0%.
      self.assertAlmostEqual(arm['inv_pct'], 50.0, places=1)

      md_hourly = report_arms.render_markdown([arm],
                                              'hourly',
                                              host='alice.c.googlers.com')
      self.assertIn('sig11 Foo', md_hourly)
      # Deterministic // run: expands the relative filename to a full path.
      self.assertIn(repro_file, md_hourly)
      self.assertIn('fatal maglev.cc:12 Check failed', md_hourly)
      self.assertIn('--bundle-reprl ' + bundle_file, md_hourly)
      self.assertIn('replays the short script sequence', md_hourly)
      self.assertIn('fatal foo.cc:99 Check failed', md_hourly)
      self.assertIn('flaky', md_hourly)
      self.assertNotIn('--stress-runs=100', md_hourly)
      self.assertIn('--stress-ic', md_hourly)
      self.assertIn('"$rc" -gt 128', md_hourly)
      self.assertIn('fatal bar.cc:7 InFlight', md_hourly)
      self.assertIn('in-flight (triaging)', md_hourly)
      self.assertNotIn('redline.log .', md_hourly)
      self.assertIn('ASAN_OPTIONS=abort_on_error=1', md_hourly)
      # Plain d8 user repro commands must not inject LD_PRELOAD=nodump.so.
      self.assertNotIn('LD_PRELOAD=/run/nodump.so', md_hourly)
      self.assertIn('run on `alice.c.googlers.com`', md_hourly)
      self.assertIn('scp alice.c.googlers.com:', md_hourly)
      self.assertIn('all imported tests failed during startup', md_hourly)

      md_imm = report_arms.render_markdown([arm], 'immediate')
      self.assertIn('arm-asan-slow', md_imm)
      self.assertIn('asan_slow_dcheck', md_imm)
      # Immediate mode shows full effective d8_args including preset flags.
      self.assertIn('--no-turbofan', md_imm)
      self.assertIn('12 h', md_imm)
      self.assertNotIn('0 edges', md_imm)
      self.assertIn('all imported tests failed during startup', md_imm)

  def test_state_dependent_edge_cases_pipe_sigs_amplified_and_non_asan(self):
    with tempfile.TemporaryDirectory() as tmp:
      arm1 = os.path.join(tmp, 'arms', 'arm-dcheck')
      crashes1 = os.path.join(arm1, 'crashes')
      bundle_min = os.path.join(crashes1, 'crash_sd_1.bundle.min.js')
      bundle_full = os.path.join(crashes1, 'crash_sd_1.bundle.js')
      self._write(bundle_min, '// minimized bundle\n')
      self._write(bundle_full, '// full bundle\n')
      self._write(
          os.path.join(crashes1, 'crash_sd_1.txt'),
          'signal: 6\nstate_dependent: yes\n'
          'bundle: crash_sd_1.bundle.js\n'
          'bundle_min: crash_sd_1.bundle.min.js\n'
          'bundle_rate: 1/4\nbundle_min_rate: 4/4\n'
          '--- stderr ---\n# Fatal error in sd.cc\n',
      )
      # Second crash of same sig where Redline skipped minimization.
      self._write(
          os.path.join(crashes1, 'crash_sd_2.txt'),
          'signal: 6\nstate_dependent: yes\n'
          'bundle: crash_sd_2.bundle.js\nbundle_min: skipped\n'
          'bundle_rate: 2/5\n--- stderr ---\n# Fatal error in sd.cc\n',
      )
      self._write(os.path.join(crashes1, 'crash_sd_2.bundle.js'), '// full 2\n')
      # Bundleless state-dependent crash.
      no_bundle_js = os.path.join(crashes1, 'crash_nobundle.js')
      self._write(no_bundle_js, '1 + 1;\n')
      self._write(
          os.path.join(crashes1, 'crash_nobundle.txt'),
          'signal: 6\nstate_dependent: yes\n'
          'bundle: no\nbundle_min: no\n',
      )
      # signatures.txt-only bug with pipe in signature + bundle_min sidecar.
      sig_bundle = os.path.join(crashes1, 'crash_sigtxt.bundle.min.js')
      self._write(sig_bundle, '// sigtxt min bundle\n')
      sig_txt = os.path.join(crashes1, 'crash_sigtxt.txt')
      self._write(
          sig_txt,
          'signal: 11\nstate_dependent: yes\n'
          'bundle_min: crash_sigtxt.bundle.min.js\n'
          'bundle_rate: 1/2\nbundle_min_rate: 3/3\n',
      )
      # signatures.txt-only amplified crash with empty amplified_args.
      amp_txt = os.path.join(crashes1, 'crash_amp.txt')
      self._write(
          amp_txt,
          'signal: 6\namplified: yes\namplified_args: \n'
          'amplified_rate: 3/5\n',
      )
      self._write(
          os.path.join(crashes1, 'signatures.txt'),
          'sig11 Foo::Bar | Baz | 3 | 1 | 0 | 1 | 42 | crash_sigtxt.js\n'
          'sig6 AmpOnly | 1 | 1 | 0 | 0 | 55 | crash_amp.js\n',
      )
      self._write(
          os.path.join(arm1, 'arm_meta.json'),
          json.dumps({
              'name': 'arm-dcheck',
              'build_variant': 'dcheck',
              'd8_path': '/home/hasan/v8/out/dcheck/d8',
              'redline_bin': '/home/hasan/v8/out/rl_bin/redline',
              'nodump_so': '/run/nodump.so',
          }),
      )
      events = [
          {
              't': 0.1,
              'event': 'start',
              'num_edges': 1000,
              'config': {
                  'd8_args': ['--future']
              },
          },
          {
              't': 50.0,
              'event': 'stats',
              'executions': 1000,
              'coverage': 0.1,
              'found_edges': 100,
              'num_edges': 1000,
          },
          {
              't': 60.0,
              'event': 'crash',
              'signature': 'fatal sd.cc Check',
              'signature_count': 1,
              'signal': 6,
              'saved': True,
              'state_dependent': True,
              'path': os.path.join(crashes1, 'crash_sd_1.js'),
          },
          {
              't': 70.0,
              'event': 'crash',
              'signature': 'fatal sd.cc Check',
              'signature_count': 2,
              'signal': 6,
              'saved': True,
              'state_dependent': True,
              'path': os.path.join(crashes1, 'crash_sd_2.js'),
          },
          {
              't': 80.0,
              'event': 'crash',
              'signature': 'fatal nobundle Check',
              'signature_count': 1,
              'signal': 6,
              'saved': True,
              'state_dependent': True,
              'path': no_bundle_js,
          },
      ]
      self._write(
          os.path.join(arm1, 'events.jsonl'),
          '\n'.join(json.dumps(e) for e in events) + '\n',
      )

      arm = report_arms.collect_arm(arm1)
      self.assertNotIn('ASAN_OPTIONS=',
                       report_arms.render_markdown([arm], 'hourly'))
      by_sig = {b['signature']: b for b in arm['bugs']}
      # Crash #2 without bundle_min kept crash #1's minimized repro + count 2.
      self.assertEqual(by_sig['fatal sd.cc Check']['count'], 2)
      self.assertIn(bundle_min, by_sig['fatal sd.cc Check']['command'])
      self.assertEqual(by_sig['fatal sd.cc Check']['repro_rate'], '4/4')
      # Bundleless state-dependent fallback is single-line REPRL exec.
      nb_cmd = by_sig['fatal nobundle Check']['command']
      self.assertNotIn('\n', nb_cmd)
      self.assertIn('/home/hasan/v8/out/rl_bin/redline exec', nb_cmd)
      self.assertIn('--d8-env=LD_PRELOAD=/run/nodump.so', nb_cmd)
      # signatures.txt fallback matched txt sidecars and escaped pipe in table.
      self.assertEqual(by_sig['sig11 Foo::Bar | Baz']['repro_rate'], '3/3')
      self.assertIn(sig_txt, by_sig['sig11 Foo::Bar | Baz']['artifacts'])
      self.assertEqual(by_sig['sig6 AmpOnly']['classification'], 'amplified')
      self.assertEqual(by_sig['sig6 AmpOnly']['repro_rate'], '3/5')

      md = report_arms.render_markdown([arm], 'hourly')
      self.assertIn('sig11 Foo::Bar \\| Baz', md)
      bug_rows = [
          ln for ln in md.splitlines()
          if ln.startswith('| ') and 'Foo::Bar' in ln
      ]
      self.assertEqual(len(bug_rows), 1)
      # Exactly 7 columns => 8 unescaped pipes in the Markdown table row.
      unescaped_pipes = len(bug_rows[0].replace('\\|', '').split('|')) - 1
      self.assertEqual(unescaped_pipes, 8)

  def test_restarted_arm_session_reset_failure_alert_and_minimized_prefix(self):
    with tempfile.TemporaryDirectory() as tmp:
      arm1 = os.path.join(tmp, 'arms', 'arm-restarted')
      crashes1 = os.path.join(arm1, 'crashes')
      crash_js = os.path.join(crashes1, 'crash_min.js')
      self._write(crash_js, '1;\n')
      self._write(
          os.path.join(crashes1, 'crash_min.txt'),
          'signal: 6\nreproducible: yes\n'
          'minimized: yes (42 tests, 100 -> 12 nodes)\n',
      )
      self._write(
          os.path.join(arm1, 'arm_meta.json'),
          json.dumps({
              'name': 'arm-restarted',
              'build_variant': 'slow_dcheck',
              'd8_path': '/v8/out/rl_slow_dcheck/d8',
          }),
      )
      # Session 1 ended cleanly; Session 2 started and later ended with failure.
      events = [
          {
              't': 0.1,
              'event': 'start',
              'num_edges': 1000,
              'config': {
                  'd8_args': []
              },
          },
          {
              't': 100.0,
              'event': 'stats',
              'executions': 5000,
              'coverage': 0.50,
              'found_edges': 500,
              'num_edges': 1000,
          },
          {
              't': 120.0,
              'event': 'end',
              'reason': 'duration',
          },
          {
              't': 0.1,
              'event': 'start',
              'num_edges': 1000,
              'config': {
                  'd8_args': []
              },
          },
          {
              't': 10.0,
              'event': 'stats',
              'executions': 200,
              'coverage': 0.10,
              'found_edges': 100,
              'num_edges': 1000,
          },
          {
              't': 20.0,
              'event': 'crash',
              'signature': 'sig6 MinPrefix',
              'signature_count': 1,
              'signal': 6,
              'saved': True,
              'reproducible': True,
              'path': crash_js,
          },
          {
              't': 30.0,
              'event': 'end',
              'reason': 'failure',
          },
      ]
      self._write(
          os.path.join(arm1, 'events.jsonl'),
          '\n'.join(json.dumps(e) for e in events) + '\n',
      )
      arm = report_arms.collect_arm(arm1)
      self.assertEqual(arm['status'], 'ENDED (failure)')
      self.assertAlmostEqual(arm['cov_delta_1h'], 0.0, places=4)
      self.assertTrue(any('arm ended with failure' in a for a in arm['alerts']))
      self.assertTrue(arm['bugs'][0]['has_min_repro'])

      # If Session 2 died without an end event, Session 1's end must not mask it.
      self._write(
          os.path.join(arm1, 'events.jsonl'),
          '\n'.join(json.dumps(e) for e in events[:-1]) + '\n',
      )
      arm_dead = report_arms.collect_arm(arm1)
      self.assertEqual(arm_dead['status'], 'STOPPED')
      self.assertTrue(
          any('without clean end event' in a for a in arm_dead['alerts']))

  def test_primary_artifact_matches_repro_command_for_scp(self):
    with tempfile.TemporaryDirectory() as tmp:
      arm1 = os.path.join(tmp, 'arms', 'arm-artifacts')
      crashes1 = os.path.join(arm1, 'crashes')
      amp_raw = os.path.join(crashes1, 'crash_amp.js')
      amp_wrapped = os.path.join(crashes1, 'crash_amp.amplified.js')
      self._write(amp_raw, 'f();\n')
      self._write(amp_wrapped, 'try { f(); } catch (e) {}\n')
      self._write(
          os.path.join(crashes1, 'crash_amp.txt'),
          'signal: 6\namplified: yes\namplified_args: --stress-runs=50\n'
          'amplified_program: crash_amp.amplified.js\namplified_rate: 4/5\n',
      )

      sd_raw = os.path.join(crashes1, 'crash_sd_both.js')
      sd_repro = os.path.join(crashes1, 'crash_sd_both.repro.js')
      sd_bundle = os.path.join(crashes1, 'crash_sd_both.bundle.js')
      self._write(sd_raw, 'g();\n')
      self._write(sd_repro, '// minimized amplified\n')
      self._write(sd_bundle, '// bundle\n')
      self._write(
          os.path.join(crashes1, 'crash_sd_both.txt'),
          'signal: 11\nstate_dependent: yes\n'
          'repro: crash_sd_both.repro.js\n'
          'bundle: crash_sd_both.bundle.js\nbundle_rate: 3/3\n',
      )
      self._write(
          os.path.join(arm1, 'arm_meta.json'),
          json.dumps({
              'name': 'arm-artifacts',
              'build_variant': 'slow_dcheck',
              'd8_path': '/v8/out/rl_slow_dcheck/d8',
          }),
      )
      events = [
          {
              't': 0.1,
              'event': 'start',
              'num_edges': 1000,
              'config': {
                  'd8_args': []
              },
          },
          {
              't': 10.0,
              'event': 'crash',
              'signature': 'sig6 AmpWrapped',
              'signature_count': 1,
              'signal': 6,
              'saved': True,
              'reproducible': False,
              'amplify': 'yes',
              'amplified_args': '--stress-runs=50',
              'path': amp_raw,
          },
          {
              't': 20.0,
              'event': 'crash',
              'signature': 'sig11 SdWithRepro',
              'signature_count': 1,
              'signal': 11,
              'saved': True,
              'state_dependent': True,
              'repro': sd_repro,
              'bundle': sd_bundle,
              'path': sd_raw,
          },
      ]
      self._write(
          os.path.join(arm1, 'events.jsonl'),
          '\n'.join(json.dumps(e) for e in events) + '\n',
      )
      arm = report_arms.collect_arm(arm1)
      by_sig = {b['signature']: b for b in arm['bugs']}
      self.assertEqual(by_sig['sig6 AmpWrapped']['artifacts'][0], amp_wrapped)
      self.assertIn(amp_wrapped, by_sig['sig6 AmpWrapped']['command'])
      self.assertEqual(by_sig['sig11 SdWithRepro']['artifacts'][0], sd_bundle)
      self.assertIn('--bundle-reprl ' + sd_bundle,
                    by_sig['sig11 SdWithRepro']['command'])

      md = report_arms.render_markdown([arm],
                                       'hourly',
                                       host='alice.c.googlers.com')
      self.assertIn(f'scp alice.c.googlers.com:{amp_wrapped} .', md)
      self.assertIn(f'scp alice.c.googlers.com:{sd_bundle} .', md)


if __name__ == '__main__':
  unittest.main()
