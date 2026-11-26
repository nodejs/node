#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for analyze_diff.py."""

import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import analyze_diff  # pylint: disable=wrong-import-position


class AnalyzeDiffTest(unittest.TestCase):

  def test_maglev_diff_plans_asan_and_change_and_turbolev(self):
    cls = analyze_diff.classify_files([
        'src/maglev/maglev-graph-builder.cc',
        'src/maglev/maglev-ir.h',
    ])
    self.assertEqual(cls['areas'], ['maglev'])
    self.assertTrue(cls['has_x64_d8_cpp'])
    self.assertFalse(cls['has_arm64_cpp'])
    self.assertFalse(cls['has_only_non_runtime'])

    arms = analyze_diff.plan_arms(cls, ['--maglev-new-opt'], vcpus=32)
    names = [a.name for a in arms]
    self.assertIn('arm-asan-slow', names)
    self.assertIn('arm-change-counters', names)
    self.assertIn('arm-turbolev-verif', names)

    asan = next(a for a in arms if a.name == 'arm-asan-slow')
    self.assertEqual(asan.build_variant, 'asan_slow_dcheck')
    self.assertFalse(asan.change_based)
    self.assertEqual(asan.coverage_mode, 'inline-bool-flag')
    self.assertIn('--maglev-new-opt', asan.d8_extra_args)
    self.assertEqual(asan.d8_flag_profile, 'maglev')
    self.assertEqual(asan.duration_sec, 43200)

    cb = next(a for a in arms if a.name == 'arm-change-counters')
    self.assertTrue(cb.change_based)
    self.assertEqual(cb.coverage_mode, 'inline-8bit-counters,pc-table')
    self.assertIn('--fuzzilli-enable-optimized-code-coverage', cb.d8_extra_args)

  def test_line_count_ranking_prevents_alphabetical_hijacking(self):
    cls = analyze_diff.classify_files(
        [
            'src/api/api.cc',
            'src/wasm/wasm-engine.cc',
            'test/mjsunit/wasm/new-edge.js',
        ],
        line_counts={
            'src/api/api.cc': 2,
            'src/wasm/wasm-engine.cc': 800,
            'test/mjsunit/wasm/new-edge.js': 40,
        },
    )
    self.assertEqual(cls['areas'][0], 'wasm')
    self.assertEqual(cls['changed_tests'], ['test/mjsunit/wasm/new-edge.js'])
    arms = analyze_diff.plan_arms(cls, [], vcpus=32)
    self.assertEqual(arms[0].preset, 'wasm')
    self.assertEqual(arms[0].d8_flag_profile, 'fuzzilli')
    self.assertTrue(arms[0].seeds.startswith('test/mjsunit/wasm/new-edge.js'))

  def test_compiler_plus_maglev_includes_maglev_arm_and_fuzzilli_profile(self):
    cls = analyze_diff.classify_files(
        [
            'src/compiler/turboshaft/foo-reducer.h',
            'src/maglev/maglev-graph-builder.cc',
        ],
        line_counts={
            'src/compiler/turboshaft/foo-reducer.h': 100,
            'src/maglev/maglev-graph-builder.cc': 60,
        },
    )
    self.assertEqual(cls['areas'], ['compiler', 'maglev'])
    arms = analyze_diff.plan_arms(cls, [], vcpus=32)
    names = [a.name for a in arms]
    self.assertIn('arm-maglev-alt', names)
    cb = next(a for a in arms if a.name == 'arm-change-counters')
    self.assertEqual(cb.d8_flag_profile, 'fuzzilli')

  def test_csa_and_torque_only_skips_x64_change_based_and_keeps_builtins_cov(
      self):
    cls = analyze_diff.classify_files([
        'src/builtins/builtins-string-gen.cc',
        'src/builtins/array.tq',
    ])
    self.assertFalse(cls['has_x64_d8_cpp'])
    self.assertTrue(cls['has_csa_or_torque'])
    self.assertFalse(cls['has_only_non_runtime'])

    arms = analyze_diff.plan_arms(cls, [], vcpus=32)
    self.assertTrue(all(not a.change_based for a in arms))
    self.assertTrue(all(a.builtins_coverage for a in arms))
    self.assertTrue(any(a.build_variant == 'asan_slow_dcheck' for a in arms))

  def test_arm64_only_triggers_arm64sim_change_based(self):
    cls = analyze_diff.classify_files([
        'src/compiler/backend/arm64/instruction-selector-arm64.cc',
    ])
    self.assertFalse(cls['has_x64_d8_cpp'])
    self.assertTrue(cls['has_arm64_cpp'])
    arms = analyze_diff.plan_arms(cls, [], vcpus=32)
    sim = next(a for a in arms if a.name == 'arm-arm64sim')
    self.assertEqual(sim.build_variant, 'arm64sim')
    self.assertTrue(sim.change_based)
    self.assertEqual(sim.coverage_mode, 'inline-8bit-counters,pc-table')

  def test_extract_new_flags_handles_multiline_shipped_debug_and_guards(self):
    diff_text = '\n'.join([
        '+++ b/src/flags/flag-definitions.h',
        '@@ -100,0 +101,6 @@',
        '+DEFINE_BOOL(maglev_cool_opt, false, "enable cool opt")',
        '+DEFINE_EXPERIMENTAL_FEATURE(',
        '+    super_mode,',
        '+    "enable super mode")',
        '+DEFINE_BOOL(debug_only_flag, false, "debug only")',
        '+++ b/src/flags/feature-flags.h',
        '@@ -10,0 +11,6 @@',
        '+  WASM_FEATURE(custom_tags, "custom tags") \\',
        '+  IF_INTL_ENABLED(JS_FEATURE, harmony_best_fit, "best fit") \\',
        '+  IF_SPARKPLUG_PLUS_ENABLED(INTERNAL_FEATURE, sp_smi, "smi") \\',
        '+  JS_FEATURE(already_shipped, "shipped feature") \\',
    ])
    flag_defs_text = '\n'.join([
        'DEFINE_BOOL(maglev_cool_opt, false, "enable cool opt")',
        'DEFINE_EXPERIMENTAL_FEATURE(',
        '    super_mode,',
        '    "enable super mode")',
        '#ifdef DEBUG',
        'DEFINE_BOOL(debug_only_flag, false, "debug only")',
        '#endif',
    ])
    feature_flags_text = '\n'.join([
        '#define FOREACH_EXPERIMENTAL_FEATURE_FLAG(JS_FEATURE, WASM_FEATURE, INTERNAL_FEATURE) \\',
        '  WASM_FEATURE(custom_tags, "custom tags") \\',
        '  IF_INTL_ENABLED(JS_FEATURE, harmony_best_fit, "best fit") \\',
        '  IF_SPARKPLUG_PLUS_ENABLED(INTERNAL_FEATURE, sp_smi, "smi")',
        '',
        '#define FOREACH_SHIPPED_FEATURE_FLAG(JS_FEATURE, WASM_FEATURE, INTERNAL_FEATURE) \\',
        '  JS_FEATURE(already_shipped, "shipped feature")',
    ])
    flags = analyze_diff.extract_new_flags(
        diff_text,
        feature_flags_text=feature_flags_text,
        flag_definitions_text=flag_defs_text,
    )
    self.assertEqual(flags, [
        '--maglev-cool-opt', '--super-mode', '--wasm-custom-tags',
        '--harmony-best-fit'
    ])

  def test_test_only_diff_flagged(self):
    cls = analyze_diff.classify_files(['test/mjsunit/foo.js', 'docs/bar.md'])
    self.assertTrue(cls['has_only_non_runtime'])
    self.assertEqual(cls['areas'], [])

  def test_wasm_sandbox_and_runtime_flag_safety(self):
    wasm_cls = analyze_diff.classify_files(['src/wasm/wasm-module.cc'])
    wasm_arms = analyze_diff.plan_arms(wasm_cls, [], vcpus=32)
    sync_arm = next(a for a in wasm_arms if a.name == 'arm-wasm-sync-verif')
    self.assertEqual(sync_arm.preset, 'wasm')
    self.assertEqual(sync_arm.d8_flag_profile, 'fuzzilli')

    sb_cls = analyze_diff.classify_files(['src/sandbox/sandbox.cc'])
    sb_arms = analyze_diff.plan_arms(sb_cls, [], vcpus=32)
    self.assertTrue(
        all('v8_enable_memory_corruption_api=true' in a.extra_gn_args
            for a in sb_arms))
    # Sandbox arms must omit --verify-heap and use fuzzilli profile to avoid
    # false-positive heap verification crashes.
    self.assertTrue(
        all('--verify-heap' not in a.d8_extra_args for a in sb_arms))
    self.assertTrue(all(a.d8_flag_profile == 'fuzzilli' for a in sb_arms))

    rt_cls = analyze_diff.classify_files(['src/objects/js-objects.cc'])
    rt_arms = analyze_diff.plan_arms(rt_cls, [], vcpus=32)
    rt_asan = next(a for a in rt_arms if a.name == 'arm-asan-slow')
    self.assertFalse(rt_asan.randomize_d8_flags)

  def test_thread_pool_budget_and_ram_cap_and_resize_plan(self):
    cls = analyze_diff.classify_files(['src/maglev/maglev-compiler.cc'])
    arms = analyze_diff.plan_arms(cls, [], vcpus=16, ram_gb=9)
    asan = next(a for a in arms if a.name == 'arm-asan-slow')
    # 9 GB RAM caps ASan workers at 3.
    self.assertLessEqual(asan.jobs, 3)
    # Effective thread usage must stay near the 85% vCPU budget.
    eff_threads = sum(a.jobs *
                      (2.0 if '--thread-pool-size=' in a.threading else 1.0)
                      for a in arms)
    self.assertLessEqual(eff_threads, 16 * 0.85 + 1.0)

    with tempfile.TemporaryDirectory() as tmp:
      plan_path = os.path.join(tmp, 'plan.json')
      with open(plan_path, 'w', encoding='utf-8') as f:
        json.dump(
            {
                'arms': [
                    {
                        'name': 'arm-asan-slow',
                        'build_variant': 'asan_slow_dcheck',
                        'change_based': False,
                        'd8_extra_args': ['--custom-user-flag'],
                        'threading': '--single-threaded',
                        'jobs': 2,
                    },
                    {
                        'name': 'arm-change-counters',
                        'build_variant': 'counters_slow_dcheck',
                        'change_based': True,
                        'd8_extra_args': ['--custom-user-flag'],
                        'threading': '--thread-pool-size=3',
                        'jobs': 2,
                    },
                ]
            }, f)
      resized = analyze_diff.resize_plan_file(plan_path, vcpus=64)
      # User edits stay intact while worker counts scale up.
      self.assertEqual(resized['arms'][0]['d8_extra_args'],
                       ['--custom-user-flag'])
      self.assertGreater(resized['arms'][1]['jobs'], 2)

  def test_questions_have_zero_redline_jargon_and_ask_duration(self):
    qs = analyze_diff._build_questions(
        areas=['maglev'],
        changed_tests=['test/mjsunit/maglev/t.js'],
        new_flags=[],
        has_x64_d8_cpp=True,
        has_arm64_cpp=False,
        has_csa_or_torque=False,
        unsupported_arches=[],
        supports_bundle_reprl=False,
        supports_v8_redline=True,
        duration_sec=43200,
    )
    joined = ' '.join(qs)
    self.assertNotIn('--bundle-reprl', joined)
    self.assertNotIn('state-dependent', joined)
    self.assertNotIn('Redline', joined)
    self.assertIn('12 h', joined)
    self.assertIn('older V8 commit', joined)

    qs_zero = analyze_diff._build_questions(
        areas=['maglev'],
        changed_tests=['test/mjsunit/maglev/t.js'],
        new_flags=[],
        has_x64_d8_cpp=True,
        has_arm64_cpp=False,
        has_csa_or_torque=False,
        unsupported_arches=[],
        supports_bundle_reprl=True,
        supports_v8_redline=False,
        duration_sec=0,
    )
    joined_zero = ' '.join(qs_zero)
    self.assertIn('No duration cap', joined_zero)
    self.assertNotIn('Default run duration is 1 h', joined_zero)
    self.assertIn('older V8 commit', joined_zero)

  def test_intl_temporal_mixed_arch_and_hand_edited_resize(self):
    intl_cls = analyze_diff.classify_files([
        'src/objects/intl-objects.cc',
        'src/builtins/builtins-intl.cc',
        'src/objects/js-break-iterator.cc',
        'src/objects/js-collator.cc',
    ])
    self.assertEqual(intl_cls['areas'], ['intl'])
    intl_arms = analyze_diff.plan_arms(intl_cls, [], vcpus=32)
    self.assertIn('test/intl', intl_arms[0].seeds)
    self.assertNotIn('test/mjsunit/intl', intl_arms[0].seeds)
    # Generated static-roots under src/gen/intl-* must not trigger intl area.
    self.assertIsNone(
        analyze_diff._area_for_path('src/gen/intl-wasm/static-roots.h'))

    temp_cls = analyze_diff.classify_files([
        'src/objects/js-temporal-objects.cc',
        'src/builtins/builtins-temporal.cc',
    ])
    self.assertEqual(temp_cls['areas'], ['temporal'])
    temp_arms = analyze_diff.plan_arms(temp_cls, [], vcpus=32)
    self.assertIn('test/mjsunit/harmony', temp_arms[0].seeds)
    self.assertNotIn('test/mjsunit/temporal', temp_arms[0].seeds)

    # Mixed x64 + riscv CL must NOT emit the unsupported-only-arch warning.
    mixed_qs = analyze_diff._build_questions(
        areas=['compiler'],
        changed_tests=[],
        new_flags=[],
        has_x64_d8_cpp=True,
        has_arm64_cpp=False,
        has_csa_or_torque=False,
        unsupported_arches=['riscv'],
        supports_bundle_reprl=True,
        supports_v8_redline=True,
        duration_sec=43200,
    )
    self.assertFalse(any('only modifies riscv' in q for q in mixed_qs))
    riscv_only_qs = analyze_diff._build_questions(
        areas=['compiler'],
        changed_tests=[],
        new_flags=[],
        has_x64_d8_cpp=False,
        has_arm64_cpp=False,
        has_csa_or_torque=False,
        unsupported_arches=['riscv'],
        supports_bundle_reprl=True,
        supports_v8_redline=True,
        duration_sec=43200,
    )
    self.assertTrue(any('only modifies riscv' in q for q in riscv_only_qs))

    # Hand-edited plan without threading field detects --thread-pool-size=.
    with tempfile.TemporaryDirectory() as tmp:
      plan_path = os.path.join(tmp, 'plan.json')
      with open(plan_path, 'w', encoding='utf-8') as f:
        json.dump(
            {
                'arms': [
                    {
                        'name': 'arm-single',
                        'build_variant': 'slow_dcheck',
                        'd8_extra_args': ['--single-threaded'],
                        'jobs': 1,
                    },
                    {
                        'name': 'arm-concurrent',
                        'build_variant': 'verif',
                        'd8_extra_args': ['--thread-pool-size=3'],
                        'jobs': 1,
                    },
                ]
            }, f)
      resized = analyze_diff.resize_plan_file(plan_path, vcpus=16)
      eff = (
          resized['arms'][0]['jobs'] * 1.0 + resized['arms'][1]['jobs'] * 2.0)
      self.assertLessEqual(eff, 16 * 0.85 + 1.0)


if __name__ == '__main__':
  unittest.main()
