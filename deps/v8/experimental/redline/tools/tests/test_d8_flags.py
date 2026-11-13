#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for d8_flag_sync.py and d8_flag_effect.py."""

import io
import json
import os
import stat
import sys
import tempfile
import unittest
from unittest import mock

TESTS_DIR = os.path.dirname(os.path.abspath(__file__))
TESTDATA_DIR = os.path.join(TESTS_DIR, 'testdata')
sys.path.insert(0, os.path.join(TESTS_DIR, '..'))
import d8_flag_effect  # pylint: disable=wrong-import-position
import d8_flag_sync  # pylint: disable=wrong-import-position

SAMPLE_TUNABLES = """\
tunables:
  flags.always_osr_from_maglev = 0.1 (default 0.1, range 0..1)  probability of --always-osr-from-maglev
  flags.future = 0.8 (default 0.8, range 0..1)  probability of --future
  flags.maglev_assert = 0.2 (default 0.2, range 0..1)  probability of --maglev-assert
  flags.shared_strings = 0.125 (default 0.125, range 0..1)  probability of --shared-strings
  flags.scavenger_chaos_mode = 0.1 (default 0.1, range 0..1)  probability of --scavenger-chaos-mode, together with --scavenger-chaos-mode-threshold=N
  flags.scavenger_chaos_mode_threshold_max = 100 (default 100, range 0..100)  largest N of --scavenger-chaos-mode-threshold=N
  flags.redline_only = 0.15 (default 0.15, range 0..1)  probability of --redline-custom-flag=N
"""

SAMPLE_TRIALS = json.dumps([
    {
        'app_args': '--future',
        'app_name': 'd8',
        'probability': 0.25,
    },
    {
        'app_args': '--no-enable-sse4_1 --trials-only=4',
        'app_name': 'd8',
        'probability': 0.05,
    },
    {
        'app_args': '--osr-from-maglev=4 --shared-string-table',
        'app_name': 'd8',
        'probability': 0.03,
    },
    {
        'app_args': '--ignored-chrome-flag',
        'app_name': 'chrome',
        'probability': 0.5,
    },
])

SAMPLE_FOOZZIE = json.dumps([
    [0.25, '--future'],
    [0.01, '--thread-pool-size=1'],
    [0.01, '--thread-pool-size=2'],
])

SAMPLE_FUZZILLI = """\
public func v8ProcessArgs(randomize: Bool, forSandbox: Bool) -> [String] {
    var args = ["--expose-gc"]
    guard randomize else { return args }
    if probability(0.8) {
        args.append("--future")
    }
    if probability(0.1) {
        args.append("--maglev-assert")
    }
    if probability(0.1) {
        args.append("--turbo-verify")
    }
    if probability(0.1) {
        args.append("--scavenger-chaos-mode")
        args.append("--scavenger-chaos-mode-threshold=\\(threshold)")
    }
    if probability(0.05) {
        func chooseBooleanFlag(_ flag: String) {
            args.append(probability(0.5) ? "--\\(flag)" : "--no-\\(flag)")
        }
        chooseBooleanFlag("maglev-inlining")
        chooseBooleanFlag("always-osr")
        chooseBooleanFlag("wasm-opt")
        chooseBooleanFlag("maglev-future")
    }
    return args
}
"""


def _read_sample_flag_defs():
  path = os.path.join(TESTDATA_DIR, 'flag_definitions_sample.h')
  with open(path, encoding='utf-8') as f:
    return f.read()


def _write_executable(path, content):
  with open(path, 'w', encoding='utf-8') as f:
    f.write(content)
  os.chmod(path, os.stat(path).st_mode | stat.S_IXUSR)


class D8FlagSyncTest(unittest.TestCase):

  def test_parse_redline_tunables(self):
    parsed = d8_flag_sync.parse_redline_tunables(SAMPLE_TUNABLES)
    self.assertEqual(
        parsed, {
            '--always-osr-from-maglev': [0.1],
            '--future': [0.8],
            '--maglev-assert': [0.2],
            '--shared-strings': [0.125],
            '--scavenger-chaos-mode': [0.1],
            '--scavenger-chaos-mode-threshold': [0.1],
            '--redline-custom-flag': [0.15],
        })

  def test_parse_bool_defaults_aliases_and_implications(self):
    sample_defs = _read_sample_flag_defs()
    defaults_release = d8_flag_sync.parse_bool_defaults(sample_defs)
    self.assertEqual(
        defaults_release, {
            'maglev-inlining': True,
            'always-osr': False,
            'wasm-opt': True,
            'turbo-verify': False,
            'macro-default-on': True,
            'macro-default-off': False,
            'maglev-future': False,
            'trace-sample': False,
            'unsafe-sample': False,
            'verify-sample': False,
        })
    defaults_debug = d8_flag_sync.parse_bool_defaults(
        sample_defs, debug_bool=True)
    self.assertIs(defaults_debug['turbo-verify'], True)
    self.assertNotIn('ambiguous-flag', defaults_debug)

    aliases = d8_flag_sync.parse_flag_aliases(sample_defs)
    self.assertEqual(aliases['--shared-string-table'], ['--shared-strings'])
    self.assertEqual(aliases['--no-shared-string-table'],
                     ['--no-shared-strings'])

    implications = d8_flag_sync.parse_flag_implications(sample_defs)
    self.assertIn(('--maglev-future', '--maglev'), implications['--maglev'])
    self.assertIn(('--jitless', '--no-opt'), implications['--no-opt'])
    self.assertIn(('--fuzzing', '--no-trace-opt'),
                  implications['--no-trace-opt'])
    self.assertIn(('--no-maglev', '--no-maglev-inlining'),
                  implications['--no-maglev-inlining'])
    self.assertIn(('--always-osr-from-maglev', '--osr-from-maglev=4'),
                  implications['--osr-from-maglev'])
    self.assertIn(('--no-turbofan', '--max-opt=2'), implications['--max-opt'])
    self.assertIn(('--no-sparkplug', '--no-always-sparkplug'),
                  implications['--no-always-sparkplug'])

  def test_parse_trials_and_foozzie(self):
    trials = d8_flag_sync.parse_trials(SAMPLE_TRIALS)
    self.assertEqual(
        trials, {
            '--future': 0.25,
            '--no-enable-sse4-1': 0.05,
            '--trials-only': 0.05,
            '--osr-from-maglev': 0.03,
            '--shared-string-table': 0.03,
        })
    foozzie = d8_flag_sync.parse_foozzie(SAMPLE_FOOZZIE)
    self.assertEqual(foozzie, {
        '--future': 0.25,
        '--thread-pool-size': 0.02,
    })

  def test_parse_fuzzilli_and_compare(self):
    sample_defs = _read_sample_flag_defs()
    redline = d8_flag_sync.parse_redline_tunables(SAMPLE_TUNABLES)
    bool_defaults = d8_flag_sync.parse_bool_defaults(
        sample_defs, debug_bool=True)
    implications = d8_flag_sync.parse_flag_implications(sample_defs)
    aliases = d8_flag_sync.parse_flag_aliases(sample_defs)
    fuzzilli = d8_flag_sync.parse_fuzzilli(SAMPLE_FUZZILLI, bool_defaults,
                                           redline)
    self.assertEqual(
        fuzzilli, {
            '--future': 0.8,
            '--maglev-assert': 0.1,
            '--turbo-verify': 0.1,
            '--scavenger-chaos-mode': 0.1,
            '--scavenger-chaos-mode-threshold': 0.1,
            '--no-maglev-inlining': 0.025,
            '--always-osr': 0.025,
            '--no-wasm-opt': 0.025,
            '--maglev-future': 0.025,
        })
    report = d8_flag_sync.compare_flags(
        redline, {
            'fuzzilli': fuzzilli,
            'trials': d8_flag_sync.parse_trials(SAMPLE_TRIALS),
            'foozzie': d8_flag_sync.parse_foozzie(SAMPLE_FOOZZIE),
        },
        bool_defaults=bool_defaults,
        implications=implications,
        aliases=aliases,
        d8_active_flags={'--always-osr', '--turbo-verify'},
        base_d8_args=['--always-osr'])
    missing_by_flag = {m['flag']: m for m in report['missing']}
    self.assertEqual(
        sorted(missing_by_flag.keys()), [
            '--always-osr',
            '--maglev-future',
            '--no-enable-sse4-1',
            '--no-maglev-inlining',
            '--no-wasm-opt',
            '--osr-from-maglev',
            '--shared-string-table',
            '--thread-pool-size',
            '--trials-only',
            '--turbo-verify',
        ])
    self.assertEqual(missing_by_flag['--always-osr']['d8_on'], 'base flags')
    self.assertEqual(missing_by_flag['--turbo-verify']['d8_on'], 'default')
    self.assertEqual(missing_by_flag['--shared-string-table']['aliases'],
                     ['--shared-strings'])
    self.assertNotIn('implied_by', missing_by_flag['--shared-string-table'])
    self.assertEqual(missing_by_flag['--osr-from-maglev']['implied_by'],
                     ['--always-osr-from-maglev -> --osr-from-maglev=4'])
    self.assertNotIn('aliases', missing_by_flag['--osr-from-maglev'])

    text = d8_flag_sync.format_report(report)
    self.assertIn('[alias of --shared-strings]', text)
    self.assertIn(
        '[implied by --always-osr-from-maglev -> --osr-from-maglev=4]', text)

  def test_real_in_tree_files_and_sample_testdata(self):
    v8_root = d8_flag_sync.V8_ROOT
    trials_path = os.path.join(v8_root, 'tools', 'clusterfuzz', 'trials',
                               'clusterfuzz_trials_config.json')
    foozzie_path = os.path.join(v8_root, 'tools', 'clusterfuzz', 'foozzie',
                                'v8_fuzz_flags.json')
    flag_defs_path = os.path.join(v8_root, 'src', 'flags', 'flag-definitions.h')
    with open(trials_path, encoding='utf-8') as f:
      trials = d8_flag_sync.parse_trials(f.read())
    with open(foozzie_path, encoding='utf-8') as f:
      foozzie = d8_flag_sync.parse_foozzie(f.read())
    with open(flag_defs_path, encoding='utf-8') as f:
      flag_defs_text = f.read()
    bool_defaults = d8_flag_sync.parse_bool_defaults(flag_defs_text)
    implications = d8_flag_sync.parse_flag_implications(flag_defs_text)
    aliases = d8_flag_sync.parse_flag_aliases(flag_defs_text)

    self.assertGreater(len(trials), 0)
    self.assertTrue(
        all(
            k.startswith('--') and '_' not in k and v > 0
            for k, v in trials.items()))
    self.assertGreater(len(foozzie), 0)
    self.assertTrue(
        all(
            k.startswith('--') and '_' not in k and v > 0
            for k, v in foozzie.items()))
    self.assertGreater(len(bool_defaults), 0)
    self.assertTrue(
        all('_' not in k and isinstance(v, bool)
            for k, v in bool_defaults.items()))
    self.assertNotIn('nam', bool_defaults)
    self.assertGreater(len(implications), 0)
    self.assertNotIn('--thenflag', implications)
    self.assertGreater(len(aliases), 0)
    self.assertNotIn('--alias', aliases)

    with open(
        os.path.join(TESTDATA_DIR, 'help_tunables.txt'), encoding='utf-8') as f:
      tunables = d8_flag_sync.parse_redline_tunables(f.read())
    self.assertIn('--always-osr-from-maglev', tunables)
    self.assertEqual(tunables['--future'], [0.8])

    with open(
        os.path.join(TESTDATA_DIR, 'print_flag_values.txt'),
        encoding='utf-8') as f:
      print_text = f.read()
    active = d8_flag_sync.parse_d8_printed_flags(print_text)
    self.assertIn('--verify-bytecode-full', active)
    self.assertIn('--maglev-assert-stack-size', active)
    printed_map = d8_flag_effect.parse_printed_flags(print_text)
    self.assertEqual(printed_map['stack-size'][0], '984')
    self.assertIs(printed_map['verify-bytecode-light'][0], False)

  def test_cli_with_fake_redline_and_fake_d8(self):
    with tempfile.TemporaryDirectory() as tmp:
      v8_root = os.path.join(tmp, 'v8')
      os.makedirs(os.path.join(v8_root, 'src', 'flags'))
      with open(
          os.path.join(v8_root, 'src', 'flags', 'flag-definitions.h'),
          'w',
          encoding='utf-8') as f:
        f.write(_read_sample_flag_defs())

      fake_redline = os.path.join(tmp, 'fake_redline.py')
      _write_executable(
          fake_redline,
          f'#!/usr/bin/env python3\nimport sys\nprint({SAMPLE_TUNABLES!r})\n')
      fake_d8 = os.path.join(tmp, 'fake_d8.py')
      _write_executable(
          fake_d8, '#!/usr/bin/env python3\n'
          'print("--always-osr\\n--no-wasm-opt\\n--efficiency-mode=true")\n')
      trials_path = os.path.join(tmp, 'trials.json')
      with open(trials_path, 'w', encoding='utf-8') as f:
        f.write(SAMPLE_TRIALS)
      foozzie_path = os.path.join(tmp, 'foozzie.json')
      with open(foozzie_path, 'w', encoding='utf-8') as f:
        f.write(SAMPLE_FOOZZIE)
      fuzzilli_path = os.path.join(tmp, 'V8CommonProfile.swift')
      with open(fuzzilli_path, 'w', encoding='utf-8') as f:
        f.write(SAMPLE_FUZZILLI)

      buf = io.StringIO()
      with mock.patch('sys.stdout', buf):
        rc = d8_flag_sync.main([
            f'--v8-root={v8_root}',
            f'--redline={fake_redline}',
            f'--d8={fake_d8}',
            f'--trials={trials_path}',
            f'--foozzie={foozzie_path}',
            f'--fuzzilli={fuzzilli_path}',
        ])
      self.assertEqual(rc, 0)
      text = buf.getvalue()
      self.assertIn('Flags missing in Redline (10):', text)
      self.assertIn('already on in d8: base flags', text)
      self.assertIn('alias of --shared-strings', text)
      self.assertIn(
          'implied by --always-osr-from-maglev -> --osr-from-maglev=4', text)
      self.assertIn('--maglev-assert: redline=0.2 (fuzzilli=0.1)', text)
      self.assertIn('Flags drawn only by Redline (3):', text)

      with mock.patch('sys.stdout', io.StringIO()):
        strict_rc = d8_flag_sync.main([
            f'--v8-root={v8_root}',
            f'--redline={fake_redline}',
            f'--trials={trials_path}',
            f'--foozzie={foozzie_path}',
            f'--fuzzilli={fuzzilli_path}',
            '--strict',
        ])
      self.assertEqual(strict_rc, 1)


class D8FlagEffectTest(unittest.TestCase):

  def test_maybe_bool_and_default_value_draws(self):
    baseline_stdout = '\n'.join([
        '--expose-gc',
        '--efficiency-mode=unset',
        '--battery-saver-mode=unset',
        '--stack-size=500',
    ])
    baseline_printed = d8_flag_effect.parse_printed_flags(baseline_stdout)
    draw_stdout = '\n'.join([
        '--expose-gc',
        '--efficiency-mode=true',
        '--battery-saver-mode=false',
        '--stack-size=500',
    ])
    contradictions, changed, noops, defaults = d8_flag_effect.check_one_run(
        ['--expose-gc'],
        ['--efficiency-mode', '--no-battery-saver-mode', '--stack-size=500'], 0,
        draw_stdout, '', baseline_printed)
    self.assertEqual(contradictions, [])
    self.assertEqual(changed, [])
    self.assertEqual(noops, [])
    self.assertEqual(defaults, ['--stack-size=500'])

    reset_stdout = '\n'.join([
        '--expose-gc',
        '--efficiency-mode=unset',
        '--battery-saver-mode=unset',
        '--stack-size=500',
    ])
    _, reset_changed, _, _ = d8_flag_effect.check_one_run(['--expose-gc'],
                                                          ['--efficiency-mode'],
                                                          0, reset_stdout, '',
                                                          baseline_printed)
    self.assertEqual(reset_changed,
                     [('--efficiency-mode', '--efficiency-mode=unset')])

  def test_check_one_run_detects_silent_change_noop_and_contradiction(self):
    baseline_stdout = '\n'.join([
        '--expose-gc',
        '--maglev',
        '--no-turbofan',
        '--jit-fuzzing',
        '--no-verify-bytecode-light',
        '--stack-size=500',
    ])
    baseline_printed = d8_flag_effect.parse_printed_flags(baseline_stdout)
    stdout = '\n'.join([
        '--expose-gc',
        '--maglev',
        '--no-turbofan',
        '--jit-fuzzing',
        '--no-verify-bytecode-light',
        '--stack-size=500',
    ])
    contradictions, changed, noops, defaults = d8_flag_effect.check_one_run(
        ['--expose-gc', '--omit-quit', '--maglev', '--no-turbofan'],
        ['--verify-bytecode-light', '--stack-size=300', '--jit-fuzzing'], 0,
        stdout, '', baseline_printed)
    self.assertEqual(contradictions, [])
    self.assertEqual(changed, [
        ('--verify-bytecode-light', '--no-verify-bytecode-light'),
        ('--stack-size=300', '--stack-size=500'),
    ])
    self.assertEqual(noops, ['--jit-fuzzing'])
    self.assertEqual(defaults, [])

    contradictions, changed, noops, defaults = d8_flag_effect.check_one_run(
        ['--maglev'], ['--bad-flag'], 1, '',
        'Flag processing error: Contradictory value for flag --bad-flag.',
        baseline_printed)
    self.assertEqual(
        contradictions,
        ['Flag processing error: Contradictory value for flag --bad-flag.'])
    self.assertEqual(changed, [])
    self.assertEqual(noops, [])
    self.assertEqual(defaults, [])

  def test_strict_allows_default_value_draws_and_handles_timeout_and_failure(
      self):
    with tempfile.TemporaryDirectory() as tmp:
      fake_redline = os.path.join(tmp, 'fake_redline.py')
      _write_executable(
          fake_redline, """#!/usr/bin/env python3
import json
import sys

if sys.argv[1] == 'fuzz' and '--dump-config' in sys.argv:
  print(json.dumps({'d8_args': ['--expose-gc']}))
elif sys.argv[1] == 'flags':
  print('--stack-size=400 --efficiency-mode')
""")

      ok_d8 = os.path.join(tmp, 'ok_d8.py')
      _write_executable(
          ok_d8, """#!/usr/bin/env python3
import sys

args = sys.argv[1:]
print('--expose-gc')
print('--stack-size=400')
if '--efficiency-mode' in args:
  print('--efficiency-mode=true')
else:
  print('--efficiency-mode=unset')
""")

      buf = io.StringIO()
      with mock.patch('sys.stdout', buf):
        rc = d8_flag_effect.main([
            f'--d8={ok_d8}',
            f'--redline={fake_redline}',
            '--presets=maglev',
            '--n=1',
            '--jobs=1',
            '--strict',
        ])
      self.assertEqual(rc, 0)
      self.assertIn('Value flags drawn with baseline value:', buf.getvalue())
      self.assertIn('--stack-size=400 (1/1 draws)', buf.getvalue())

      bad_base_d8 = os.path.join(tmp, 'bad_base_d8.py')
      _write_executable(
          bad_base_d8, """#!/usr/bin/env python3
import sys

sys.stderr.write('Error: unrecognized flag --broken-base\\n')
sys.exit(1)
""")
      err_buf = io.StringIO()
      with mock.patch('sys.stderr', err_buf):
        fail_rc = d8_flag_effect.main([
            f'--d8={bad_base_d8}',
            f'--redline={fake_redline}',
            '--presets=maglev',
            '--n=1',
            '--jobs=1',
        ])
      self.assertEqual(fail_rc, 2)
      self.assertIn('Baseline d8 run failed for preset maglev',
                    err_buf.getvalue())

      slow_draw_d8 = os.path.join(tmp, 'slow_draw_d8.py')
      _write_executable(
          slow_draw_d8, """#!/usr/bin/env python3
import sys
import time

if '--efficiency-mode' in sys.argv[1:]:
  time.sleep(2.0)
print('--expose-gc')
print('--stack-size=400')
print('--efficiency-mode=unset')
""")
      timeout_buf = io.StringIO()
      with mock.patch('sys.stdout', timeout_buf):
        timeout_rc = d8_flag_effect.main([
            f'--d8={slow_draw_d8}',
            f'--redline={fake_redline}',
            '--presets=maglev',
            '--n=1',
            '--jobs=1',
            '--timeout=0.1',
            '--strict',
        ])
      self.assertEqual(timeout_rc, 1)
      self.assertIn('Error: d8 timed out after 0.1s (1/1 draws)',
                    timeout_buf.getvalue())

  def test_cli_with_fake_d8_and_fake_redline(self):
    with tempfile.TemporaryDirectory() as tmp:
      fake_redline = os.path.join(tmp, 'fake_redline.py')
      _write_executable(
          fake_redline, """#!/usr/bin/env python3
import json
import sys

if sys.argv[1] == 'fuzz' and '--dump-config' in sys.argv:
  preset = [a.split('=', 1)[1] for a in sys.argv if a.startswith('--preset=')][0]
  extra = ['--maglev', '--no-turbofan'] if preset == 'maglev' else ['--turbolev']
  print(json.dumps({'d8_args': ['--expose-gc', '--omit-quit'] + extra}))
elif sys.argv[1] == 'flags':
  preset = [a.split('=', 1)[1] for a in sys.argv if a.startswith('--preset=')][0]
  if preset == 'maglev':
    print('--verify-bytecode-light --stack-size=400')
    print('--future --stack-size=400')
  else:
    print('--verify-turboshaft')
    print('--stress-concurrent-inlining')
""")

      fake_d8 = os.path.join(tmp, 'fake_d8.py')
      _write_executable(
          fake_d8, """#!/usr/bin/env python3
import sys

args = sys.argv[1:]
# Check production order: drawn flags precede `--expose-gc`.
if '--future' in args and args.index('--future') > args.index('--expose-gc'):
  sys.exit(2)
if '--stress-concurrent-inlining' in args:
  sys.stderr.write(
      'Warning: resetting flag --stress-concurrent-inlining '
      'due to conflicting flags\\n')
print('--expose-gc')
print('--maglev' if '--maglev' in args else '--no-maglev')
print('--no-turbofan' if '--no-turbofan' in args else '--turbofan')
print('--turbolev' if '--turbolev' in args else '--no-turbolev')
print('--future' if '--future' in args else '--no-future')
print('--verify-turboshaft' if '--verify-turboshaft' in args else '--no-verify-turboshaft')
print('--no-stress-concurrent-inlining')
print('--no-verify-bytecode-light')
print('--stack-size=400')
""")

      buf = io.StringIO()
      with mock.patch('sys.stdout', buf):
        rc = d8_flag_effect.main([
            f'--d8={fake_d8}',
            f'--redline={fake_redline}',
            '--n=2',
            '--seed=1',
            '--jobs=1',
        ])
      self.assertEqual(rc, 0)
      output = buf.getvalue()
      self.assertIn(
          '--verify-bytecode-light -> --no-verify-bytecode-light (1/2 draws)',
          output)
      self.assertIn('Value flags drawn with baseline value:', output)
      self.assertIn('--stack-size=400 (2/2 draws)', output)
      self.assertIn(
          'Warning: resetting flag --stress-concurrent-inlining '
          'due to conflicting flags (1/2 draws)', output)

      with mock.patch('sys.stdout', io.StringIO()):
        strict_rc = d8_flag_effect.main([
            f'--d8={fake_d8}',
            f'--redline={fake_redline}',
            '--n=2',
            '--seed=1',
            '--jobs=1',
            '--strict',
        ])
      self.assertEqual(strict_rc, 1)


if __name__ == '__main__':
  unittest.main()
