#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for triage_report.py."""

from __future__ import annotations

import contextlib
import io
import json
import os
from pathlib import Path
import re
import stat
import sys
import tempfile
import unittest
from unittest import mock

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
  sys.path.insert(0, str(SCRIPT_DIR))

import triage_report as tr  # pylint: disable=g-import-not-at-top


class TriageTestBase(unittest.TestCase):
  """Base test case providing a temporary directory and executable d8 helper."""

  def setUp(self) -> None:
    super().setUp()
    self._tmp = tempfile.TemporaryDirectory()
    self.temp_dir = Path(self._tmp.name)
    self.default_poc = self.temp_dir / "poc.js"
    self.default_poc.write_text("// poc\n", encoding="utf-8")
    self.default_d8 = self._write_d8(
        'if [[ "$*" == *"--run-as-sandbox-security-poc"* ]]; then\n'
        '  echo "## V8 sandbox violation detected!" >&2\n'
        '  exit 134\n'
        'fi\n'
        'echo "# Fatal error in Maglev"\n'
        'exit 134\n')
    self.default_sandbox_d8 = self._write_d8(
        'if [[ "$*" == *"--run-as-sandbox-security-poc"* ]]; then\n'
        '  echo "## V8 sandbox violation detected!" >&2\n'
        '  exit 134\n'
        'fi\n'
        'exit 1\n',
        name="d8_sandbox",
    )

  def tearDown(self) -> None:
    self._tmp.cleanup()
    super().tearDown()

  def _write_d8(self, body: str, name: str = "d8") -> Path:
    d8_path = self.temp_dir / name
    d8_path.write_text(
        f"#!/usr/bin/env bash\nulimit -c 0\n{body}\n", encoding="utf-8")
    d8_path.chmod(d8_path.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP
                  | stat.S_IXOTH)
    return d8_path

  def _base_data(self, symptom: str = "oob_write", **overrides) -> dict:
    data = {
        "session_id":
            "test-session-123",
        "symptom":
            symptom,
        "sandbox_mode":
            False,
        "affects_out_of_sandbox_state":
            False,
        "reproduction": {
            "d8_binary":
                str(self.default_d8),
            "poc_path":
                str(self.default_poc),
            "d8_flags": ["--run-as-security-poc"],
            "unsupported_port":
                False,
            "build_variants": ["x64.release", "x64.asan"],
            "build_commit":
                "HEAD",
            "result":
                "SIGSEGV writing out-of-bounds to JSArray elements.",
            "verified_impact":
                "Out-of-bounds write of 8-byte attacker-controlled value in the V8 heap.",
            "gdb_backtrace": [
                "#0  0x0000555556789abc in Builtins_Maglev...",
                "#1  0x0000555556123456 in v8::internal::Execution::Call...",
            ],
        },
        "introduced_in_commit":
            "HEAD",
        "fixed_in_commit":
            None,
        "rationale": (
            "Maglev bounds check elimination elides the check across a side-effecting call. "
            "An attacker can shrink the array inside the callback to trigger an out-of-bounds write."
        ),
        "proposed_owner": {
            "email": "owner@chromium.org",
            "reason": "author of introducing commit and subsystem maintainer",
        },
        "proposed_component":
            "src/maglev",
        "proposed_title":
            "[maglev] Preserve bounds check across side-effecting call",
        "current_issue": {
            "id": "123456789",
            "title": "Security: crash in d8",
            "component_id": "1456800",
            "priority": "P1",
            "severity": "S1",
            "hotlists": [tr.HOTLIST_SECURITY_IMPACT_HEAD],
            "has_release_block": True,
        },
    }
    if symptom == "not_reproduced":
      data["reproduction"]["d8_binary"] = None
      data["reproduction"]["verified_impact"] = None
      data["reproduction"]["gdb_backtrace"] = None
    elif overrides.get("sandbox_mode"):
      data["reproduction"]["d8_binary"] = str(self.default_sandbox_d8)
    data.update(overrides)
    return data


class SymptomMatrixTest(TriageTestBase):
  """Tests the 12x3 (Symptom, ExecutionScope) matrix via real subprocess execution."""

  def test_symptom_table_completeness(self) -> None:
    forbidden_policy_tokens = (
        "S1",
        "S2",
        "S3",
        "Vulnerability",
        "Intended Behavior",
        "WAI",
    )
    for symptom in tr.Symptom:
      self.assertIn(symptom, tr.SYMPTOM_EMPIRICAL_GUIDE)
      guide_text = tr.SYMPTOM_EMPIRICAL_GUIDE[symptom].strip()
      self.assertTrue(guide_text)
      for token in forbidden_policy_tokens:
        self.assertNotIn(token, guide_text)
      for scope in tr.ExecutionScope:
        self.assertIn((symptom, scope), tr.SYMPTOM_RULES)
    self.assertIn(
        "actual prior heap, object, or IR state corruption in Release",
        tr.SYMPTOM_EMPIRICAL_GUIDE[tr.Symptom.VERIFICATION_CHECK_FAILURE],
    )
    self.assertIn(
        "OBJECT_PRINT",
        tr.SYMPTOM_EMPIRICAL_GUIDE[tr.Symptom.DCHECK_BENIGN],
    )

  def test_regular_mode_symptom_matrix(self) -> None:
    expected = {
        "oob_write": (tr.Classification.VULNERABILITY, "S1",
                      tr.SecurityImpact.YES, "P1"),
        "uaf": (tr.Classification.VULNERABILITY, "S1", tr.SecurityImpact.YES,
                "P1"),
        "type_confusion": (tr.Classification.VULNERABILITY, "S1",
                           tr.SecurityImpact.YES, "P1"),
        "oob_read": (tr.Classification.VULNERABILITY, "S3",
                     tr.SecurityImpact.YES, "P2"),
        "uninitialized_read": (tr.Classification.VULNERABILITY, "S3",
                               tr.SecurityImpact.YES, "P2"),
        "dcheck_corrupts_state": (tr.Classification.VULNERABILITY, "S1",
                                  tr.SecurityImpact.YES, "P1"),
        "dcheck_benign":
            (tr.Classification.BUG, None, tr.SecurityImpact.NONE, "P2"),
        "verification_check_failure": (tr.Classification.VULNERABILITY, "S1",
                                       tr.SecurityImpact.YES, "P1"),
        "production_check_or_trap":
            (tr.Classification.BUG, None, tr.SecurityImpact.NONE, "P2"),
        "nullptr_deref":
            (tr.Classification.BUG, None, tr.SecurityImpact.NONE, "P2"),
        "hole_leak":
            (tr.Classification.BUG, None, tr.SecurityImpact.NONE, "P2"),
        "not_reproduced": (tr.Classification.FAILED_TO_REPRODUCE, None,
                           tr.SecurityImpact.NONE, "P3"),
    }
    for symptom_val, (exp_cls, exp_sev, exp_imp, exp_prio) in expected.items():
      with self.subTest(symptom=symptom_val):
        data = self._base_data(symptom=symptom_val)
        errors = tr.validate_triage_input(data, repo_root=self.temp_dir)
        self.assertEqual(errors, [])
        dec = tr.compute_triage_decision(data, repo_root=self.temp_dir)
        self.assertEqual(dec.classification, exp_cls)
        self.assertEqual(dec.severity, exp_sev)
        self.assertEqual(dec.security_impact, exp_imp)
        self.assertEqual(dec.priority, exp_prio)

  def test_sandbox_in_sandbox_only_matrix(self) -> None:
    for symptom in tr.Symptom:
      with self.subTest(symptom=symptom.value):
        data = self._base_data(
            symptom=symptom.value,
            sandbox_mode=True,
            affects_out_of_sandbox_state=False,
        )
        errors = tr.validate_triage_input(data, repo_root=self.temp_dir)
        self.assertEqual(errors, [])
        dec = tr.compute_triage_decision(data, repo_root=self.temp_dir)
        exp_cls = (
            tr.Classification.FAILED_TO_REPRODUCE
            if symptom == tr.Symptom.NOT_REPRODUCED else
            tr.Classification.INTENDED_BEHAVIOR)
        self.assertEqual(dec.classification, exp_cls)
        self.assertIsNone(dec.severity)
        self.assertEqual(dec.security_impact, tr.SecurityImpact.NONE)
        self.assertEqual(dec.priority, "P3")

  def test_sandbox_out_of_sandbox_escape_matrix(self) -> None:
    escape_symptoms = {
        tr.Symptom.OOB_WRITE,
        tr.Symptom.UAF,
        tr.Symptom.TYPE_CONFUSION,
        tr.Symptom.DCHECK_CORRUPTS_STATE,
        tr.Symptom.VERIFICATION_CHECK_FAILURE,
    }
    read_bypass_symptoms = {
        tr.Symptom.OOB_READ,
        tr.Symptom.UNINITIALIZED_READ,
    }
    for symptom in tr.Symptom:
      if symptom == tr.Symptom.NOT_REPRODUCED:
        continue
      with self.subTest(symptom=symptom.value):
        data = self._base_data(
            symptom=symptom.value,
            sandbox_mode=True,
            affects_out_of_sandbox_state=True,
        )
        errors = tr.validate_triage_input(data, repo_root=self.temp_dir)
        self.assertEqual(errors, [])
        dec = tr.compute_triage_decision(data, repo_root=self.temp_dir)
        if symptom in escape_symptoms:
          self.assertEqual(dec.classification, tr.Classification.VULNERABILITY)
          self.assertEqual(dec.severity, "S2")
          self.assertEqual(dec.security_impact, tr.SecurityImpact.YES)
          self.assertEqual(dec.priority, "P2")
          added_ids = [h[0] for h in dec.hotlists_to_add]
          self.assertIn(tr.HOTLIST_V8_SANDBOX, added_ids)
          self.assertNotIn(tr.HOTLIST_SECURITY_IMPACT_NONE, added_ids)
          self.assertEqual(dec.hotlists_to_remove, [])
          self.assertFalse(dec.clear_release_block)
        elif symptom in read_bypass_symptoms:
          self.assertEqual(dec.classification, tr.Classification.BUG)
          self.assertIsNone(dec.severity)
          self.assertEqual(dec.security_impact, tr.SecurityImpact.NONE)
          self.assertEqual(dec.priority, "P2")
        else:
          self.assertEqual(dec.classification,
                           tr.Classification.INTENDED_BEHAVIOR)
          self.assertIsNone(dec.severity)
          self.assertEqual(dec.security_impact, tr.SecurityImpact.NONE)
          self.assertEqual(dec.priority, "P3")


class EmpiricalPocRunnerSubprocessTest(TriageTestBase):
  """Tests the 2-run EmpiricalPocRunner with zero flag allowlists."""

  def test_no_flag_allowlist_or_output_regexes_exist(self) -> None:
    for removed_attr in (
        "ALLOWED_PRODUCTION_HELPER_FLAGS",
        "_is_stage1_helper_arg",
        "_FLAG_REJECTION_RE",
        "_NON_SECURITY_EXIT_RE",
        "_HARMLESS_SANDBOX_RE",
        "_CRASH_OUTPUT_RE",
        "count_sentences",
        "build_triage_output_json",
    ):
      self.assertFalse(hasattr(tr, removed_attr), removed_attr)

  def test_run1_flagless_reproduces_and_drops_superfluous_flags(self) -> None:
    d8 = self._write_d8('if [[ "$*" == *"--run-as-security-poc"* ]]; then\n'
                        '  exit 139\n'
                        'fi\n'
                        'exit 0\n')
    runner = tr.EmpiricalPocRunner(repo_root=self.temp_dir)
    res = runner.run(
        str(d8),
        str(self.default_poc),
        ["--run-as-security-poc", "--allow-natives-syntax", "--future"],
        sandbox_mode=False,
    )
    self.assertTrue(res.reproduces_with_security_poc)
    self.assertTrue(res.enabled_by_default)
    self.assertEqual(res.matched_stage, "run1_flagless")
    self.assertEqual(res.effective_d8_flags, ["--run-as-security-poc"])
    self.assertEqual(len(res.stages), 1)
    self.assertEqual(res.stages[0].returncode, 139)
    self.assertEqual(res.stages[0].command,
                     ["d8", "--run-as-security-poc", "poc.js"])

  def test_run2_with_flags_reproduces_when_run1_does_not_crash(self) -> None:
    d8 = self._write_d8(
        'if [[ "$*" == *"--run-as-security-poc"* && "$*" == *"--js-staging"* ]]; then\n'
        '  exit 134\n'
        'fi\n'
        'exit 0\n')
    runner = tr.EmpiricalPocRunner(repo_root=self.temp_dir)
    res = runner.run(
        str(d8),
        str(self.default_poc),
        ["--js-staging"],
        sandbox_mode=False,
    )
    self.assertTrue(res.reproduces_with_security_poc)
    self.assertFalse(res.enabled_by_default)
    self.assertEqual(res.matched_stage, "run2_with_flags")
    self.assertEqual(res.effective_d8_flags,
                     ["--run-as-security-poc", "--js-staging"])
    self.assertEqual(len(res.stages), 2)
    self.assertFalse(res.stages[0].reproduced)
    self.assertTrue(res.stages[1].reproduced)

  def test_poc_helper_flags_keep_enabled_by_default_true(self) -> None:
    d8 = self._write_d8(
        'if [[ "$*" == *"--run-as-security-poc"* && "$*" == *"--expose-gc"* ]]; then\n'
        '  if [[ "$ASAN_OPTIONS" == *"abort_on_error=1"* ]]; then\n'
        '    exit 134\n'
        '  fi\n'
        '  exit 1\n'
        'fi\n'
        'exit 0\n')
    runner = tr.EmpiricalPocRunner(repo_root=self.temp_dir)
    res = runner.run(
        str(d8),
        str(self.default_poc),
        ["--expose-gc", "--allow-natives-syntax", "--verify-heap"],
        sandbox_mode=False,
    )
    self.assertTrue(res.reproduces_with_security_poc)
    self.assertTrue(res.enabled_by_default)
    self.assertEqual(res.matched_stage, "run1_with_poc_helpers")
    self.assertEqual(
        res.effective_d8_flags,
        [
            "--run-as-security-poc",
            "--expose-gc",
            "--allow-natives-syntax",
            "--verify-heap",
        ],
    )

  def test_neither_run1_nor_run2_reproduces_on_exit_255_or_exit_1(self) -> None:
    d8 = self._write_d8('if [[ "$*" == *"--bad-dev-flag"* ]]; then\n'
                        '  exit 255\n'
                        'fi\n'
                        'exit 1\n')
    runner = tr.EmpiricalPocRunner(repo_root=self.temp_dir)
    res = runner.run(
        str(d8), str(self.default_poc), ["--bad-dev-flag"], sandbox_mode=False)
    self.assertFalse(res.reproduces_with_security_poc)
    self.assertFalse(res.enabled_by_default)
    self.assertIsNone(res.matched_stage)
    self.assertEqual(res.effective_d8_flags, ["--bad-dev-flag"])
    self.assertEqual(len(res.stages), 2)
    self.assertEqual(res.stages[0].returncode, 1)
    self.assertEqual(res.stages[1].returncode, 255)

  def test_sandbox_mode_strips_memory_corruption_api_and_checks_exit_code(
      self) -> None:
    d8 = self._write_d8(
        'if [[ "$*" == *"--expose-memory-corruption-api"* ]]; then\n'
        '  exit 255\n'
        'fi\n'
        'if [[ "$*" == *"--run-as-sandbox-security-poc"* ]]; then\n'
        '  echo "## V8 sandbox violation detected!" >&2\n'
        '  exit 139\n'
        'fi\n'
        'exit 0\n')
    runner = tr.EmpiricalPocRunner(repo_root=self.temp_dir)
    res = runner.run(
        str(d8),
        str(self.default_poc),
        ["--expose-memory-corruption-api"],
        sandbox_mode=True,
    )
    self.assertTrue(res.reproduces_with_security_poc)
    self.assertTrue(res.enabled_by_default)
    self.assertEqual(res.matched_stage, "run1_flagless")
    self.assertEqual(res.effective_d8_flags, ["--run-as-sandbox-security-poc"])
    self.assertEqual(len(res.stages), 1)
    self.assertEqual(res.stages[0].returncode, 139)

  def test_sandbox_mode_harmless_catch_exits_zero_and_is_not_reproduced(
      self) -> None:
    d8 = self._write_d8("exit 0\n")
    runner = tr.EmpiricalPocRunner(repo_root=self.temp_dir)
    res = runner.run(str(d8), str(self.default_poc), [], sandbox_mode=True)
    self.assertFalse(res.reproduces_with_security_poc)
    self.assertFalse(res.enabled_by_default)
    self.assertIsNone(res.matched_stage)
    self.assertEqual(res.effective_d8_flags, ["--run-as-sandbox-security-poc"])
    self.assertFalse(res.flag_rejected)

  def test_sandbox_mode_failure_preserves_sandbox_flag_and_final_stage_harmless(
      self) -> None:
    # Earlier stage hits harmless catch without flag; final stage with
    # required flag exits 0 cleanly (not a harmless catch).
    d8_mixed = self._write_d8(
        'if [[ "$*" == *"--wasm-staging"* ]]; then\n'
        '  exit 0\n'
        'fi\n'
        'echo "Caught harmless signal (SIGABRT). Exiting process..." >&2\n'
        'exit 0\n',
        name="d8_sbx_mixed",
    )
    runner = tr.EmpiricalPocRunner(repo_root=self.temp_dir)
    res_mixed = runner.run(
        str(d8_mixed),
        str(self.default_poc),
        ["--wasm-staging"],
        sandbox_mode=True,
    )
    self.assertFalse(res_mixed.reproduces_with_security_poc)
    self.assertFalse(res_mixed.sandbox_harmless_catch)
    self.assertEqual(res_mixed.effective_d8_flags,
                     ["--run-as-sandbox-security-poc", "--wasm-staging"])

    # Rejected flag (exit 255) preserves `--sandbox-testing` so the
    # `Reproduction:` command still exposes the `Sandbox` API.
    d8_rej = self._write_d8(
        'if [[ "$*" == *"--bad-flag"* ]]; then\n'
        '  exit 255\n'
        'fi\n'
        'exit 0\n',
        name="d8_sbx_rej",
    )
    res_rej = runner.run(
        str(d8_rej),
        str(self.default_poc),
        ["--bad-flag"],
        sandbox_mode=True,
    )
    self.assertFalse(res_rej.reproduces_with_security_poc)
    self.assertTrue(res_rej.flag_rejected)
    self.assertEqual(res_rej.effective_d8_flags,
                     ["--sandbox-testing", "--bad-flag"])

    # Flagless exit 255 (e.g. FatalOOM) is NOT treated as a rejected flag.
    d8_oom = self._write_d8("exit 255\n", name="d8_sbx_oom")
    res_oom = runner.run(
        str(d8_oom),
        str(self.default_poc),
        [],
        sandbox_mode=True,
    )
    self.assertFalse(res_oom.reproduces_with_security_poc)
    self.assertFalse(res_oom.flag_rejected)
    self.assertEqual(res_oom.effective_d8_flags,
                     ["--run-as-sandbox-security-poc"])

  def test_sandbox_crash_filter_stderr_only_and_linux_violation_banner(
      self) -> None:
    runner = tr.EmpiricalPocRunner(repo_root=self.temp_dir)

    # 1. JS stdout printing read-only/harmless text must not spoof stderr checks.
    d8_spoof = self._write_d8(
        'echo "The sandbox violation was a *read* access"\n'
        'echo "Caught harmless signal. Exiting process..."\n'
        'echo "## V8 sandbox violation detected!" >&2\n'
        'exit 139\n',
        name="d8_sbx_spoof",
    )
    res_spoof = runner.run(
        str(d8_spoof), str(self.default_poc), [], sandbox_mode=True)
    self.assertTrue(res_spoof.reproduces_with_security_poc)
    self.assertFalse(res_spoof.sandbox_read_only)
    self.assertFalse(res_spoof.sandbox_harmless_catch)

    # 2. ASan FilterCrash without "Caught harmless " prefix matches via
    #    `FilterCrash`'s shared `" Exiting process..."` stderr suffix on exit 0.
    d8_asan_filtered = self._write_d8(
        'echo "Caught ASan fault without a fault address. Ignoring it as we cannot check if it is a sandbox violation. Exiting process..." >&2\n'
        'exit 0\n',
        name="d8_sbx_asan_filtered",
    )
    res_asan = runner.run(
        str(d8_asan_filtered), str(self.default_poc), [], sandbox_mode=True)
    self.assertFalse(res_asan.reproduces_with_security_poc)
    self.assertTrue(res_asan.sandbox_harmless_catch)

    # 3. Non-violation signal exit (e.g. SIGFPE / OOM SIGKILL without sandbox
    #    violation banner on stderr) is not treated as reproduced on Linux,
    #    while non-Linux platforms (without InstallCrashFilter) still accept it.
    d8_non_sbx_sig = self._write_d8("exit 136\n", name="d8_non_sbx_sig")
    with mock.patch.object(tr.sys, "platform", "linux"):
      res_linux = runner.run(
          str(d8_non_sbx_sig), str(self.default_poc), [], sandbox_mode=True)
    self.assertFalse(res_linux.reproduces_with_security_poc)
    with mock.patch.object(tr.sys, "platform", "darwin"):
      res_mac = runner.run(
          str(d8_non_sbx_sig), str(self.default_poc), [], sandbox_mode=True)
    self.assertTrue(res_mac.reproduces_with_security_poc)

  def test_timeout_handling(self) -> None:
    d8 = self._write_d8("sleep 2\nexit 0\n")
    runner = tr.EmpiricalPocRunner(timeout_sec=0.05, repo_root=self.temp_dir)
    res = runner.run(str(d8), str(self.default_poc), [], sandbox_mode=False)
    self.assertFalse(res.reproduces_with_security_poc)
    self.assertTrue(res.stages[0].timed_out)


class EmpiricalOverridesTest(TriageTestBase):
  """Tests classification, priority, and hotlist overrides from EmpiricalPocRunner."""

  def test_regular_mode_not_reproducing_with_security_poc_downgrades_to_bug(
      self) -> None:
    d8 = self._write_d8("exit 0\n")
    data = self._base_data(symptom="oob_write")
    data["reproduction"]["d8_binary"] = str(d8)
    data["reproduction"]["d8_flags"] = ["--allow-natives-syntax"]
    dec = tr.compute_triage_decision(data, repo_root=self.temp_dir)
    self.assertEqual(dec.classification, tr.Classification.BUG)
    self.assertIsNone(dec.severity)
    self.assertEqual(dec.security_impact, tr.SecurityImpact.NONE)
    self.assertEqual(dec.priority, "P2")
    self.assertFalse(dec.reproduces_with_security_poc)
    self.assertFalse(dec.enabled_by_default)
    self.assertEqual(dec.effective_d8_flags, ["--allow-natives-syntax"])
    self.assertTrue(dec.clear_release_block)

  def test_sandbox_mode_not_reproducing_with_security_poc_downgrades_to_wai(
      self) -> None:
    d8 = self._write_d8("exit 0\n")
    data = self._base_data(
        symptom="oob_write",
        sandbox_mode=True,
        affects_out_of_sandbox_state=True,
    )
    data["reproduction"]["d8_binary"] = str(d8)
    dec = tr.compute_triage_decision(data, repo_root=self.temp_dir)
    self.assertEqual(dec.classification, tr.Classification.INTENDED_BEHAVIOR)
    self.assertIsNone(dec.severity)
    self.assertEqual(dec.security_impact, tr.SecurityImpact.NONE)
    self.assertEqual(dec.priority, "P3")
    self.assertFalse(dec.reproduces_with_security_poc)

  def test_sandbox_mode_overridden_when_reproducing_in_regular_mode(
      self) -> None:
    data = self._base_data(
        symptom="oob_write",
        sandbox_mode=True,
        affects_out_of_sandbox_state=False,
    )
    # `default_d8` crashes under `--run-as-security-poc` (regular mode), so even
    # though `sandbox_mode=True` and `affects_out_of_sandbox_state=False` were
    # passed, it must be classified as a regular S1/P1 Vulnerability instead of
    # being dismissed as Intended Behavior.
    data["reproduction"]["d8_binary"] = str(self.default_d8)
    dec = tr.compute_triage_decision(data, repo_root=self.temp_dir)
    self.assertEqual(dec.execution_scope, tr.ExecutionScope.REGULAR)
    self.assertEqual(dec.classification, tr.Classification.VULNERABILITY)
    self.assertEqual(dec.severity, "S1")
    self.assertEqual(dec.security_impact, tr.SecurityImpact.YES)
    self.assertEqual(dec.priority, "P1")
    self.assertEqual(dec.effective_d8_flags, ["--run-as-security-poc"])

    # Conversely, if the model forgets to strip `--sandbox-testing` or
    # `--expose-memory-corruption-api` from `d8_flags`, the regular-mode probe
    # must not pass those flags in Run 2 and misclassify a sandbox bypass.
    sbx_d8 = self._write_d8(
        'if [[ "$*" == *"--run-as-sandbox-security-poc"* || "$*" == *"--sandbox-testing"* ]]; then\n'
        '  echo "## V8 sandbox violation detected!" >&2\n'
        '  exit 139\n'
        'fi\n'
        'exit 0\n',
        name="d8_sbx_only",
    )
    sbx_data = self._base_data(
        symptom="oob_write",
        sandbox_mode=True,
        affects_out_of_sandbox_state=True,
    )
    sbx_data["reproduction"]["d8_binary"] = str(sbx_d8)
    sbx_data["reproduction"]["d8_flags"] = [
        "--sandbox-testing",
        "--expose-memory-corruption-api",
    ]
    sbx_dec = tr.compute_triage_decision(sbx_data, repo_root=self.temp_dir)
    self.assertEqual(sbx_dec.execution_scope,
                     tr.ExecutionScope.SANDBOX_OUT_OF_SANDBOX)
    self.assertEqual(sbx_dec.classification, tr.Classification.VULNERABILITY)
    self.assertEqual(sbx_dec.severity, "S2")
    self.assertEqual(sbx_dec.security_impact, tr.SecurityImpact.YES)
    self.assertEqual(sbx_dec.priority, "P2")
    self.assertTrue(sbx_dec.enabled_by_default)
    self.assertEqual(sbx_dec.effective_d8_flags,
                     ["--run-as-sandbox-security-poc"])

  def test_run2_with_flags_downgrades_security_impact_to_none(self) -> None:
    d8 = self._write_d8(
        'if [[ "$*" == *"--run-as-security-poc"* && "$*" == *"--future"* ]]; then\n'
        '  echo "# Fatal error"\n'
        '  exit 134\n'
        'fi\n'
        'exit 0\n')
    for symptom, exp_sev, exp_prio in (("oob_write", "S1", "P2"), ("oob_read",
                                                                   "S3", "P3")):
      with self.subTest(symptom=symptom):
        data = self._base_data(symptom=symptom)
        data["reproduction"]["d8_binary"] = str(d8)
        data["reproduction"]["d8_flags"] = ["--future"]
        dec = tr.compute_triage_decision(data, repo_root=self.temp_dir)
        self.assertEqual(dec.classification, tr.Classification.VULNERABILITY)
        self.assertEqual(dec.severity, exp_sev)
        self.assertEqual(dec.security_impact, tr.SecurityImpact.NONE)
        self.assertEqual(dec.priority, exp_prio)
        self.assertTrue(dec.reproduces_with_security_poc)
        self.assertFalse(dec.enabled_by_default)
        self.assertEqual(dec.effective_d8_flags,
                         ["--run-as-security-poc", "--future"])
        added_ids = [h[0] for h in dec.hotlists_to_add]
        self.assertIn(tr.HOTLIST_SECURITY_IMPACT_NONE, added_ids)
        self.assertTrue(dec.clear_release_block)

  def test_unsupported_port_sets_impact_none_and_adds_hotlist(self) -> None:
    data = self._base_data(symptom="oob_write")
    data["reproduction"]["unsupported_port"] = True
    dec = tr.compute_triage_decision(data, repo_root=self.temp_dir)
    self.assertEqual(dec.classification, tr.Classification.VULNERABILITY)
    self.assertEqual(dec.severity, "S1")
    self.assertEqual(dec.security_impact, tr.SecurityImpact.NONE)
    self.assertEqual(dec.priority, "P2")
    added_ids = [h[0] for h in dec.hotlists_to_add]
    self.assertIn(tr.HOTLIST_V8_UNSUPPORTED_CHROME, added_ids)
    self.assertIn(tr.HOTLIST_SECURITY_IMPACT_NONE, added_ids)

  def test_enable_inspector_override_vs_superfluous(self) -> None:
    d8_inspector_only = self._write_d8(
        'if [[ "$*" == *"--enable-inspector"* ]]; then\n'
        '  echo "# Fatal error"\n'
        '  exit 134\n'
        'fi\n'
        'exit 0\n',
        name="d8_inspector_only",
    )
    data = self._base_data(symptom="oob_write")
    data["reproduction"]["d8_binary"] = str(d8_inspector_only)
    data["reproduction"]["d8_flags"] = ["--enable-inspector"]
    dec = tr.compute_triage_decision(data, repo_root=self.temp_dir)
    self.assertEqual(dec.classification, tr.Classification.BUG)
    self.assertIsNone(dec.severity)
    self.assertEqual(dec.security_impact, tr.SecurityImpact.NONE)
    self.assertEqual(dec.priority, "P2")
    self.assertEqual(dec.component_id, tr.DEVTOOLS_COMPONENT_ID)
    self.assertEqual(dec.component_path, tr.DEVTOOLS_COMPONENT_PATH)
    self.assertEqual(dec.effective_d8_flags, ["--enable-inspector"])
    self.assertIn("https://crbug.com/573042262", dec.security_impact_rationale)
    self.assertNotIn("b/573042262", dec.security_impact_rationale)

    data2 = self._base_data(symptom="oob_write")
    data2["reproduction"]["d8_flags"] = ["--enable-inspector"]
    dec2 = tr.compute_triage_decision(data2, repo_root=self.temp_dir)
    self.assertEqual(dec2.classification, tr.Classification.VULNERABILITY)
    self.assertEqual(dec2.severity, "S1")
    self.assertEqual(dec2.security_impact, tr.SecurityImpact.YES)
    self.assertEqual(dec2.priority, "P1")
    self.assertEqual(dec2.effective_d8_flags, ["--run-as-security-poc"])

    data3 = self._base_data(symptom="oob_write")
    data3["reproduction"]["d8_binary"] = str(d8_inspector_only)
    data3["reproduction"]["d8_flags"] = ["--enable-inspector"]
    data3["current_issue"]["component_id"] = tr.DEVTOOLS_COMPONENT_ID
    dec3 = tr.compute_triage_decision(data3, repo_root=self.temp_dir)
    self.assertEqual(dec3.component_id, tr.DEVTOOLS_COMPONENT_ID)
    self.assertTrue(dec3.component_already_set)

  def test_sandbox_read_only_banner_downgrades_to_bug(self) -> None:
    d8_sbx_read = self._write_d8(
        'if [[ "$*" == *"--run-as-sandbox-security-poc"* ]]; then\n'
        '  echo "## V8 sandbox violation detected!" >&2\n'
        '  echo "The sandbox violation was a *read* access which is technically not a sandbox violation." >&2\n'
        '  exit 139\n'
        'fi\n'
        'echo "ReferenceError: Sandbox is not defined" >&2\n'
        'exit 1\n',
        name="d8_sbx_read",
    )
    data = self._base_data(
        symptom="oob_write",
        sandbox_mode=True,
        affects_out_of_sandbox_state=True,
    )
    data["reproduction"]["d8_binary"] = str(d8_sbx_read)
    dec = tr.compute_triage_decision(data, repo_root=self.temp_dir)
    self.assertEqual(dec.classification, tr.Classification.BUG)
    self.assertIsNone(dec.severity)
    self.assertEqual(dec.security_impact, tr.SecurityImpact.NONE)
    self.assertEqual(dec.priority, "P2")
    self.assertIn("*read*", dec.security_impact_rationale)

  def test_d8_only_or_embedder_mock_downgrades_to_bug(self) -> None:
    d8_ext_only = self._write_d8(
        'if [[ "$*" == *"--use-external-strings"* ]]; then\n'
        '  exit 134\n'
        'fi\n'
        'exit 0\n',
        name="d8_ext_only",
    )
    data = self._base_data(symptom="oob_write")
    data["reproduction"]["d8_binary"] = str(d8_ext_only)
    data["reproduction"]["d8_only_or_embedder_mock"] = True
    data["reproduction"]["d8_flags"] = ["--use-external-strings"]
    dec = tr.compute_triage_decision(data, repo_root=self.temp_dir)
    self.assertEqual(dec.classification, tr.Classification.BUG)
    self.assertIsNone(dec.severity)
    self.assertEqual(dec.security_impact, tr.SecurityImpact.NONE)
    self.assertEqual(dec.priority, "P2")
    self.assertFalse(dec.enabled_by_default)
    self.assertEqual(dec.empirical_run["matched_stage"], "run2_with_flags")

    # Even if a mock-only POC crashes during flagless run1, keep
    # `enabled_by_default=False`.
    flagless_mock = self._base_data(symptom="oob_write")
    flagless_mock["reproduction"]["d8_only_or_embedder_mock"] = True
    flagless_dec = tr.compute_triage_decision(
        flagless_mock, repo_root=self.temp_dir)
    self.assertEqual(flagless_dec.classification, tr.Classification.BUG)
    self.assertFalse(flagless_dec.enabled_by_default)

    # Sandbox violation caused only by d8/mock flag becomes Bug, not Vulnerability.
    d8_sbx_ext = self._write_d8(
        'if [[ "$*" == *"--run-as-sandbox-security-poc"* &&'
        ' "$*" == *"--use-external-strings"* ]]; then\n'
        '  echo "## V8 sandbox violation detected!" >&2\n'
        '  exit 139\n'
        'fi\n'
        'exit 0\n',
        name="d8_sbx_ext",
    )
    sbx_data = self._base_data(
        symptom="oob_write",
        sandbox_mode=True,
        affects_out_of_sandbox_state=True,
    )
    sbx_data["reproduction"]["d8_binary"] = str(d8_sbx_ext)
    sbx_data["reproduction"]["d8_flags"] = ["--use-external-strings"]
    sbx_data["reproduction"]["d8_only_or_embedder_mock"] = True
    sbx_dec = tr.compute_triage_decision(sbx_data, repo_root=self.temp_dir)
    self.assertEqual(sbx_dec.execution_scope,
                     tr.ExecutionScope.SANDBOX_OUT_OF_SANDBOX)
    self.assertEqual(sbx_dec.classification, tr.Classification.BUG)
    self.assertIsNone(sbx_dec.severity)
    self.assertEqual(sbx_dec.security_impact, tr.SecurityImpact.NONE)
    self.assertEqual(sbx_dec.priority, "P2")
    self.assertFalse(sbx_dec.enabled_by_default)

    # Caught-harmless sandbox crash stays Intended Behavior (P3), even when
    # `d8_only_or_embedder_mock` and `affects_out_of_sandbox_state` are set.
    d8_sbx_harmless = self._write_d8(
        'echo "Caught harmless signal (SIGABRT). Exiting process..." >&2\n'
        'exit 0\n',
        name="d8_sbx_harmless",
    )
    harmless_data = self._base_data(
        symptom="oob_write",
        sandbox_mode=True,
        affects_out_of_sandbox_state=True,
    )
    harmless_data["reproduction"]["d8_binary"] = str(d8_sbx_harmless)
    harmless_data["reproduction"]["d8_flags"] = ["--use-external-strings"]
    harmless_data["reproduction"]["d8_only_or_embedder_mock"] = True
    harmless_dec = tr.compute_triage_decision(
        harmless_data, repo_root=self.temp_dir)
    self.assertEqual(harmless_dec.classification,
                     tr.Classification.INTENDED_BEHAVIOR)
    self.assertIsNone(harmless_dec.severity)
    self.assertEqual(harmless_dec.security_impact, tr.SecurityImpact.NONE)
    self.assertEqual(harmless_dec.priority, "P3")
    self.assertFalse(harmless_dec.enabled_by_default)
    self.assertIn("caught harmless", harmless_dec.security_impact_rationale)

    # Rejected d8-only flag (exit 255) in sandbox_mode with out-of-sandbox
    # impact becomes Bug (P2), not Intended Behavior (P3).
    d8_sbx_rej = self._write_d8(
        'if [[ "$*" == *"--simulate-errors"* ]]; then\n'
        '  exit 255\n'
        'fi\n'
        'exit 0\n',
        name="d8_sbx_d8only_rej",
    )
    rej_mock = self._base_data(
        symptom="oob_write",
        sandbox_mode=True,
        affects_out_of_sandbox_state=True,
    )
    rej_mock["reproduction"]["d8_binary"] = str(d8_sbx_rej)
    rej_mock["reproduction"]["d8_flags"] = ["--simulate-errors"]
    rej_mock["reproduction"]["d8_only_or_embedder_mock"] = True
    rej_mock_dec = tr.compute_triage_decision(rej_mock, repo_root=self.temp_dir)
    self.assertEqual(rej_mock_dec.classification, tr.Classification.BUG)
    self.assertIsNone(rej_mock_dec.severity)
    self.assertEqual(rej_mock_dec.security_impact, tr.SecurityImpact.NONE)
    self.assertEqual(rej_mock_dec.priority, "P2")
    self.assertFalse(rej_mock_dec.enabled_by_default)
    self.assertEqual(rej_mock_dec.effective_d8_flags,
                     ["--sandbox-testing", "--simulate-errors"])

    # In-sandbox-only POC stays Intended Behavior even if flag is set.
    in_sbx = self._base_data(
        symptom="oob_write",
        sandbox_mode=True,
        affects_out_of_sandbox_state=False,
    )
    in_sbx["reproduction"]["d8_binary"] = str(self.default_sandbox_d8)
    in_sbx["reproduction"]["d8_only_or_embedder_mock"] = True
    in_sbx_dec = tr.compute_triage_decision(in_sbx, repo_root=self.temp_dir)
    self.assertEqual(in_sbx_dec.classification,
                     tr.Classification.INTENDED_BEHAVIOR)
    self.assertEqual(in_sbx_dec.priority, "P3")
    self.assertFalse(in_sbx_dec.enabled_by_default)

  def test_sandbox_mode_rejected_flag_downgrades_oos_to_bug_not_wai(
      self) -> None:
    d8_rej = self._write_d8(
        'if [[ "$*" == *"--experimental-foo"* ]]; then\n'
        '  exit 255\n'
        'fi\n'
        'exit 0\n',
        name="d8_sbx_exp_rej",
    )
    oos_data = self._base_data(
        symptom="oob_write",
        sandbox_mode=True,
        affects_out_of_sandbox_state=True,
    )
    oos_data["reproduction"]["d8_binary"] = str(d8_rej)
    oos_data["reproduction"]["d8_flags"] = ["--experimental-foo"]
    oos_dec = tr.compute_triage_decision(oos_data, repo_root=self.temp_dir)
    self.assertEqual(oos_dec.classification, tr.Classification.BUG)
    self.assertIsNone(oos_dec.severity)
    self.assertEqual(oos_dec.security_impact, tr.SecurityImpact.NONE)
    self.assertEqual(oos_dec.priority, "P2")
    self.assertEqual(oos_dec.effective_d8_flags,
                     ["--sandbox-testing", "--experimental-foo"])

    # In-sandbox-only rejected flag stays Intended Behavior (P3).
    in_data = self._base_data(
        symptom="oob_write",
        sandbox_mode=True,
        affects_out_of_sandbox_state=False,
    )
    in_data["reproduction"]["d8_binary"] = str(d8_rej)
    in_data["reproduction"]["d8_flags"] = ["--experimental-foo"]
    in_dec = tr.compute_triage_decision(in_data, repo_root=self.temp_dir)
    self.assertEqual(in_dec.classification, tr.Classification.INTENDED_BEHAVIOR)
    self.assertEqual(in_dec.priority, "P3")

    # Flagless exit 255 (e.g. FatalOOM) stays Intended Behavior (P3).
    d8_oom = self._write_d8("exit 255\n", name="d8_sbx_flagless_255")
    oom_data = self._base_data(
        symptom="oob_write",
        sandbox_mode=True,
        affects_out_of_sandbox_state=True,
    )
    oom_data["reproduction"]["d8_binary"] = str(d8_oom)
    oom_data["reproduction"]["d8_flags"] = []
    oom_dec = tr.compute_triage_decision(oom_data, repo_root=self.temp_dir)
    self.assertEqual(oom_dec.classification,
                     tr.Classification.INTENDED_BEHAVIOR)
    self.assertEqual(oom_dec.priority, "P3")

  def test_unconfirmed_vulnerability_and_blocked_steps(self) -> None:
    d8_clean = self._write_d8("exit 0\n", name="d8_clean")
    data = self._base_data(symptom="oob_write")
    data["reproduction"]["d8_binary"] = str(d8_clean)
    data["reproduction"]["d8_flags"] = []
    data["reproduction"]["unconfirmed_vulnerability"] = True
    data["blocked_steps"] = [
        "Crash escalation blocked by internal safety filter."
    ]
    self.assertEqual(
        tr.validate_triage_input(data, repo_root=self.temp_dir), [])
    dec = tr.compute_triage_decision(data, repo_root=self.temp_dir)
    self.assertEqual(dec.classification,
                     tr.Classification.TENTATIVE_VULNERABILITY)
    self.assertTrue(dec.unconfirmed_crash)
    self.assertEqual(dec.severity, "S1")
    self.assertEqual(dec.priority, "P1")
    self.assertEqual(dec.security_impact, tr.SecurityImpact.YES)
    report = tr.render_triage_report(data, dec,
                                     tr.GitVersionResolver(self.temp_dir))
    self.assertIn("- **Classification**: `Tentative Vulnerability`", report)
    self.assertIn("`Verification Limitations`:", report)
    self.assertIn("Unconfirmed crash:", report)
    self.assertIn("Crash escalation blocked by internal safety filter.", report)
    self.assertNotIn("`update_issue_type`: `Tentative Vulnerability`", report)

    # Harmless sandbox catch is never promoted to Tentative Vulnerability.
    d8_harmless = self._write_d8(
        'echo "Caught harmless signal (SIGABRT). Exiting process..." >&2\n'
        'exit 0\n',
        name="d8_harmless",
    )
    sbx_data = self._base_data(
        symptom="oob_write",
        sandbox_mode=True,
        affects_out_of_sandbox_state=True,
    )
    sbx_data["reproduction"]["d8_binary"] = str(d8_harmless)
    sbx_data["reproduction"]["unconfirmed_vulnerability"] = True
    sbx_dec = tr.compute_triage_decision(sbx_data, repo_root=self.temp_dir)
    self.assertEqual(sbx_dec.classification,
                     tr.Classification.INTENDED_BEHAVIOR)
    self.assertFalse(sbx_dec.unconfirmed_crash)
    self.assertEqual(sbx_dec.effective_d8_flags,
                     ["--run-as-sandbox-security-poc"])

    # Silent sandbox-mode unconfirmed vulnerability preserves sandbox flag in
    # `Reproduction:` and keeps `SecurityImpact.YES` (S2/P2) when flagless.
    sbx_unconf = self._base_data(
        symptom="oob_write",
        sandbox_mode=True,
        affects_out_of_sandbox_state=True,
    )
    sbx_unconf["reproduction"]["d8_binary"] = str(d8_clean)
    sbx_unconf["reproduction"]["d8_flags"] = []
    sbx_unconf["reproduction"]["unconfirmed_vulnerability"] = True
    sbx_unconf_dec = tr.compute_triage_decision(
        sbx_unconf, repo_root=self.temp_dir)
    self.assertEqual(sbx_unconf_dec.classification,
                     tr.Classification.TENTATIVE_VULNERABILITY)
    self.assertEqual(sbx_unconf_dec.severity, "S2")
    self.assertEqual(sbx_unconf_dec.security_impact, tr.SecurityImpact.YES)
    self.assertEqual(sbx_unconf_dec.priority, "P2")
    self.assertEqual(sbx_unconf_dec.effective_d8_flags,
                     ["--run-as-sandbox-security-poc"])

    # Rejected flag (exit 255) downgrades to Bug, not Tentative Vulnerability.
    d8_rejected = self._write_d8("exit 255\n", name="d8_rejected")
    rej_data = self._base_data(symptom="oob_write")
    rej_data["reproduction"]["d8_binary"] = str(d8_rejected)
    rej_data["reproduction"]["d8_flags"] = ["--experimental-foo"]
    rej_data["reproduction"]["unconfirmed_vulnerability"] = True
    rej_dec = tr.compute_triage_decision(rej_data, repo_root=self.temp_dir)
    self.assertEqual(rej_dec.classification, tr.Classification.BUG)
    self.assertFalse(rej_dec.unconfirmed_crash)

    # Uncaught JS error (exit 1) or timeout (returncode is None) must NOT
    # promote to Tentative Vulnerability.
    d8_err = self._write_d8("exit 1\n", name="d8_js_err")
    err_data = self._base_data(symptom="oob_write")
    err_data["reproduction"]["d8_binary"] = str(d8_err)
    err_data["reproduction"]["unconfirmed_vulnerability"] = True
    err_dec = tr.compute_triage_decision(err_data, repo_root=self.temp_dir)
    self.assertEqual(err_dec.classification, tr.Classification.BUG)
    self.assertFalse(err_dec.unconfirmed_crash)

    d8_slow = self._write_d8("sleep 2\nexit 0\n", name="d8_slow_unconf")
    slow_data = self._base_data(symptom="oob_write")
    slow_data["reproduction"]["d8_binary"] = str(d8_slow)
    slow_data["reproduction"]["unconfirmed_vulnerability"] = True
    slow_dec = tr.compute_triage_decision(
        slow_data, repo_root=self.temp_dir, poc_timeout=0.05)
    self.assertEqual(slow_dec.classification, tr.Classification.BUG)
    self.assertFalse(slow_dec.unconfirmed_crash)

    # Crashing POC with blocked_steps alone becomes Tentative Vulnerability.
    blk_data = self._base_data(symptom="oob_write")
    blk_data["blocked_steps"] = ["GDB step timed out before wrap-up."]
    blk_dec = tr.compute_triage_decision(blk_data, repo_root=self.temp_dir)
    self.assertEqual(blk_dec.classification,
                     tr.Classification.TENTATIVE_VULNERABILITY)
    self.assertFalse(blk_dec.unconfirmed_crash)

  def test_regular_probe_skips_extra_stages_when_sandbox_undefined(
      self) -> None:
    d8 = self._write_d8(
        'if [[ "$*" == *"--run-as-sandbox-security-poc"* ]]; then\n'
        '  exit 139\n'
        'fi\n'
        'echo "ReferenceError: Sandbox is not defined" >&2\n'
        'exit 1\n',
        name="d8_sbx_undef",
    )
    runner = tr.EmpiricalPocRunner(repo_root=self.temp_dir)
    res = runner.run(
        str(d8),
        str(self.default_poc),
        ["--allow-natives-syntax", "--future"],
        sandbox_mode=False,
        skip_on_sandbox_undefined=True,
    )
    self.assertFalse(res.reproduces_with_security_poc)
    self.assertEqual(len(res.stages), 1)
    self.assertTrue(res.stages[0].sandbox_undefined_error)

  def test_ubsan_env_sets_halt_and_abort_on_error(self) -> None:
    with mock.patch.dict("os.environ", {}, clear=True):
      env = tr.EmpiricalPocRunner._build_env()
      self.assertEqual(env["ASAN_OPTIONS"], "abort_on_error=1")
      self.assertIn("abort_on_error=1", env["UBSAN_OPTIONS"])
      self.assertIn("halt_on_error=1", env["UBSAN_OPTIONS"])


class ValidationAndRenderingTest(TriageTestBase):
  """Tests schema validation, Markdown rendering, CLI execution, and SKILL.md invariants."""

  def test_offline_boolean_fields_rejected_by_validation(self) -> None:
    for removed_key in (
        "reproduces_with_security_poc_flag",
        "enabled_by_default",
        "empirical_run",
        "effective_d8_flags",
    ):
      with self.subTest(removed_key=removed_key):
        data = self._base_data()
        data["reproduction"][removed_key] = True
        errors = tr.validate_triage_input(data, repo_root=self.temp_dir)
        self.assertTrue(
            any("Unknown `reproduction` JSON field" in e for e in errors),
            errors)

  def test_d8_binary_always_required_when_reproduced(self) -> None:
    data = self._base_data(symptom="oob_write")
    data["reproduction"]["d8_binary"] = None
    errors = tr.validate_triage_input(data, repo_root=self.temp_dir)
    self.assertTrue(any("reproduction.d8_binary" in e for e in errors), errors)

  def test_sandbox_mode_requires_affects_out_of_sandbox_state(self) -> None:
    data = self._base_data(symptom="oob_write", sandbox_mode=True)
    del data["affects_out_of_sandbox_state"]
    errors = tr.validate_triage_input(data, repo_root=self.temp_dir)
    self.assertTrue(
        any("affects_out_of_sandbox_state" in e for e in errors), errors)

    data2 = self._base_data(
        symptom="oob_write",
        sandbox_mode=False,
        affects_out_of_sandbox_state=True)
    errors2 = tr.validate_triage_input(data2, repo_root=self.temp_dir)
    self.assertTrue(
        any("cannot be `true` when `sandbox_mode` is `false`" in e
            for e in errors2), errors2)

  def test_local_path_detection_and_brevity_validation(self) -> None:
    data = self._base_data()
    data["rationale"] = "Referencing local path /tmp/poc.js."
    data["reproduction"]["verified_impact"] = "x" * 400
    errors = tr.validate_triage_input(data, repo_root=self.temp_dir)
    self.assertTrue(
        any("local workstation/temp path" in e for e in errors), errors)
    self.assertTrue(
        any("`reproduction.verified_impact` must be concise" in e
            for e in errors), errors)

    # Single-sentence rationale with no local paths and relative build_commit (`deadbeef~1`) succeeds
    data2 = self._base_data()
    data2[
        "rationale"] = "Single concise sentence with C++ symbols like v8::internal::Maglev."
    data2["reproduction"]["build_commit"] = "0ac2b27dda5~1"
    errors2 = tr.validate_triage_input(data2, repo_root=self.temp_dir)
    self.assertEqual(errors2, [])

  def test_clear_release_block_only_when_has_release_block_is_true(
      self) -> None:
    data = self._base_data(symptom="dcheck_benign")
    data["current_issue"]["has_release_block"] = False
    dec = tr.compute_triage_decision(data, repo_root=self.temp_dir)
    self.assertFalse(dec.clear_release_block)

    data["current_issue"]["has_release_block"] = True
    dec2 = tr.compute_triage_decision(data, repo_root=self.temp_dir)
    self.assertTrue(dec2.clear_release_block)

  def test_render_triage_report_and_cli(self) -> None:
    data = self._base_data()
    errors = tr.validate_triage_input(data, repo_root=self.temp_dir)
    self.assertEqual(errors, [])
    dec = tr.compute_triage_decision(data, repo_root=self.temp_dir)
    resolver = tr.GitVersionResolver(self.temp_dir)
    report = tr.render_triage_report(data, dec, resolver)
    self.assertIn("using the `v8-security-triaging` skill", report)
    self.assertIn("- **Classification**: `Vulnerability`", report)
    self.assertIn("- **Security Impact**: **Yes**", report)
    self.assertIn("- `Reproduction`: `d8 --run-as-security-poc poc.js`", report)
    self.assertIn("--- (User Only: Do Not Post to Buganizer) ---", report)
    self.assertIn("### Empirical Security POC Verification (User Only)", report)
    self.assertNotIn("ClusterFuzz Upload Info", report)
    self.assertNotIn("\u2014", report)

    bap_report = tr.render_triage_report(data, dec, resolver, output_mode="bap")
    self.assertIn("using the `v8-security-triaging` skill", bap_report)
    self.assertNotIn("--- (User Only: Do Not Post to Buganizer) ---",
                     bap_report)
    self.assertNotIn("Buganizer MCP Action Plan", bap_report)
    self.assertNotIn("add_buganizer_comment", bap_report)

    with self.assertRaises(ValueError):
      tr.render_triage_report(data, dec, resolver, output_mode="invalid")

    epilog = tr.format_help_epilog()
    self.assertIn("Empirical Symptom Guide (`symptom`):", epilog)
    self.assertIn("Empirical Sandbox Scope Guide", epilog)
    self.assertIn("Empirical `reproduction.d8_binary` Selection Guide:", epilog)
    self.assertNotIn("reproduces_with_security_poc_flag", epilog)
    self.assertNotIn("enabled_by_default", epilog)

    parsed_tpl = tr.load_lenient_json(tr.JSON_TEMPLATE)
    self.assertNotIn("clusterfuzz", parsed_tpl)
    self.assertIn("title", parsed_tpl["current_issue"])

    # Verify trailing-comma stripping does not corrupt commas inside JSON strings
    tricky_json = '{\n  "a": "hello, ] world, }", // comment\n  "b": [1, 2, ],\n}'
    parsed_tricky = tr.load_lenient_json(tricky_json)
    self.assertEqual(parsed_tricky["a"], "hello, ] world, }")
    self.assertEqual(parsed_tricky["b"], [1, 2])

  def test_detect_default_repo_root_from_cwd(self) -> None:
    fake_v8 = (self.temp_dir / "fake_v8").resolve()
    header_dir = fake_v8 / "include"
    header_dir.mkdir(parents=True)
    (header_dir / "v8-version.h").write_text(
        "#define V8_MAJOR_VERSION 13\n", encoding="utf-8")
    subdir = fake_v8 / "out" / "x64.release"
    subdir.mkdir(parents=True)
    with mock.patch.object(Path, "cwd", return_value=subdir):
      self.assertEqual(tr._detect_default_repo_root(), fake_v8)
      self.assertEqual(tr.EmpiricalPocRunner().repo_root, fake_v8)

    # Also safe when the single-file script is copied to a shallow path (e.g.
    # /tmp/triage_report.py) while working inside an older checkout.
    non_v8 = (self.temp_dir / "elsewhere").resolve()
    non_v8.mkdir()
    with mock.patch.object(
        Path, "cwd",
        return_value=non_v8), mock.patch.object(tr, "__file__",
                                                "/tmp/triage_report.py"):
      self.assertEqual(tr._detect_default_repo_root(), non_v8)

    copied = fake_v8 / "tmp" / "triage_report.py"
    copied.parent.mkdir()
    with mock.patch.object(
        Path, "cwd",
        return_value=non_v8), mock.patch.object(tr, "__file__", str(copied)):
      self.assertEqual(tr._detect_default_repo_root(), fake_v8)

  def test_resolve_path_prefers_repo_root_over_cwd_for_worktrees(self) -> None:
    main_repo = (self.temp_dir / "v8").resolve()
    worktree = main_repo / "worktrees" / "task-cr-123"
    main_d8 = main_repo / "out" / "x64.release" / "d8"
    wt_d8 = worktree / "out" / "x64.release" / "d8"
    for p in (main_d8, wt_d8):
      p.parent.mkdir(parents=True, exist_ok=True)
      p.write_text("#!/bin/sh\n", encoding="utf-8")

    runner = tr.EmpiricalPocRunner(repo_root=worktree)
    orig_cwd = Path.cwd()
    try:
      os.chdir(main_repo)
      # Relative path inside JSON must resolve against `--repo-root` worktree,
      # not `cwd` (the main repo checkout).
      self.assertEqual(runner.resolve_path("out/x64.release/d8"), wt_d8)
      # Pre-prefixed cwd-relative paths still resolve cleanly.
      self.assertEqual(
          runner.resolve_path("worktrees/task-cr-123/out/x64.release/d8"),
          wt_d8)
      # Absolute paths remain untouched.
      self.assertEqual(runner.resolve_path(str(main_d8)), main_d8)
      # Missing worktree binary must NOT silently fall back to main checkout.
      wt_d8.unlink()
      self.assertEqual(runner.resolve_path("out/x64.release/d8"), wt_d8)
      self.assertFalse(runner.resolve_path("out/x64.release/d8").exists())
    finally:
      os.chdir(orig_cwd)

  def test_main_resolves_relative_input_json_against_repo_root(self) -> None:
    main_repo = (self.temp_dir / "main_v8").resolve()
    worktree = main_repo / "worktrees" / "task-cr-777"
    (worktree / "include").mkdir(parents=True)
    (worktree / "include" / "v8-version.h").write_text(
        "#define V8_MAJOR_VERSION 13\n", encoding="utf-8")
    (worktree / "src" / "maglev").mkdir(parents=True)
    (worktree / "src" / "maglev" / "DIR_METADATA").write_text(
        'buganizer_public {\n  component_id: 1456177\n}\n'
        'monorail {\n  component: "Blink>JavaScript>Compiler>Maglev"\n}\n',
        encoding="utf-8",
    )
    wt_tmp = worktree / "tmp"
    wt_tmp.mkdir()
    wt_poc = wt_tmp / "crash.js"
    wt_poc.write_text("// poc\n", encoding="utf-8")
    data = self._base_data(symptom="oob_write")
    data["reproduction"]["d8_binary"] = str(self.default_d8)
    data["reproduction"]["poc_path"] = "tmp/crash.js"
    data["proposed_title"] = "[maglev] worktree test title"
    (wt_tmp / "triage_input.json").write_text(
        json.dumps(data), encoding="utf-8")
    # Put stale/wrong JSON inside main checkout to confirm `--repo-root` wins.
    (main_repo / "tmp").mkdir()
    (main_repo / "tmp" / "triage_input.json").write_text(
        "{invalid", encoding="utf-8")

    orig_cwd = Path.cwd()
    stdout_buf = io.StringIO()
    try:
      os.chdir(main_repo)
      with contextlib.redirect_stdout(stdout_buf):
        rc = tr.main([
            "--repo-root",
            "worktrees/task-cr-777",
            "--output-mode=bap",
            "tmp/triage_input.json",
        ])
    finally:
      os.chdir(orig_cwd)
    self.assertEqual(rc, 0)
    self.assertIn("[maglev] worktree test title", stdout_buf.getvalue())
    self.assertIn("`d8 --run-as-security-poc crash.js`", stdout_buf.getvalue())

    # Nonexistent `--repo-root` fails loudly instead of falling back to cwd.
    stderr_buf = io.StringIO()
    with contextlib.redirect_stderr(stderr_buf):
      bad_rc = tr.main([
          "--repo-root",
          str(self.temp_dir / "does_not_exist"),
          "tmp/triage_input.json",
      ])
    self.assertEqual(bad_rc, 1)
    self.assertIn("INVALID --repo-root", stderr_buf.getvalue())

  def test_dynamic_dir_metadata_and_component_id_resolution(self) -> None:
    self.assertFalse(hasattr(tr, "_V8_COMPONENT_ENTRIES"))
    data = self._base_data()
    # 1. Resolve from V8 source directory via DIR_METADATA
    data["proposed_component"] = "src/maglev"
    data["current_issue"]["component_id"] = "1456800"
    errors = tr.validate_triage_input(data, repo_root=self.temp_dir)
    self.assertEqual(errors, [])
    dec = tr.compute_triage_decision(data, repo_root=self.temp_dir)
    self.assertEqual(dec.component_path,
                     "Blink > JavaScript > Compiler > Maglev")
    self.assertEqual(dec.component_id, "1456177")
    self.assertFalse(dec.component_already_set)

    # 2. Resolve from numeric component_id (auto-matching current_issue.component_id)
    data["proposed_component"] = "1456177"
    data["current_issue"]["component_id"] = "1456177"
    dec2 = tr.compute_triage_decision(data, repo_root=self.temp_dir)
    self.assertEqual(dec2.component_path,
                     "Blink > JavaScript > Compiler > Maglev")
    self.assertEqual(dec2.component_id, "1456177")
    self.assertTrue(dec2.component_already_set)

    # 3. Resolve from a numeric component_id not in DIR_METADATA (e.g. Turbofan 1456959)
    data["proposed_component"] = "1456959"
    data["current_issue"]["component_id"] = "1456800"
    dec3 = tr.compute_triage_decision(data, repo_root=self.temp_dir)
    self.assertEqual(dec3.component_path, "")
    self.assertEqual(dec3.component_id, "1456959")
    self.assertFalse(dec3.component_already_set)
    report3 = tr.render_triage_report(data, dec3,
                                      tr.GitVersionResolver(self.temp_dir))
    self.assertIn("- **Proposed Component**: ID `1456959`", report3)

  def test_duplicate_of_and_fixed_in_commit_streamlined_action_plan(
      self) -> None:
    data = self._base_data()
    data["proposed_component"] = "Maglev"
    data["current_issue"]["component_id"] = "1456800"
    data["fixed_in_commit"] = "0ac2b27dda5"
    resolver = tr.GitVersionResolver(self.temp_dir)
    with mock.patch.object(
        resolver, "get_commit_fixed_bugs", return_value=["574412712"]):
      dec = tr.compute_triage_decision(data, repo_root=self.temp_dir)
      report = tr.render_triage_report(data, dec, resolver)
      self.assertIn(
          "- **Classification**: `Vulnerability` (Duplicate of https://crbug.com/574412712)",
          report)
      self.assertNotIn("b/574412712", report)
      self.assertIn("`mark_issue_as_duplicate`", report)
      self.assertNotIn("`update_issue_type`", report)
      self.assertNotIn("`safe_reassign_issue`", report)

  def test_fixed_trailer_only_for_auto_duplicate_ignores_bug_trailer(
      self) -> None:
    resolver = tr.GitVersionResolver(self.temp_dir)
    with mock.patch.object(
        resolver,
        "_run_git",
        side_effect=[
            "deadbeef",
            "[maglev] Minor cleanup\n\nBug: 42200001\n",
            "deadbeef",
            "[maglev] Fix issue\n\nFixed: 574412712\nBug: 42200001\n",
        ],
    ):
      self.assertEqual(resolver.get_commit_fixed_bugs("deadbeef"), [])
      self.assertEqual(
          resolver.get_commit_fixed_bugs("deadbeef"), ["574412712"])

  def test_validation_rejects_bad_optional_fields(self) -> None:
    data = self._base_data(symptom="nullptr_deref")
    data["reproduction"]["unconfirmed_vulnerability"] = True
    data["blocked_steps"] = ["Failed reading /usr/local/secret/poc.js"]
    data["severity_override"] = {"severity": "S2", "reason": "disallowed"}
    errors = tr.validate_triage_input(data, repo_root=self.temp_dir)
    self.assertTrue(
        any("unconfirmed_vulnerability" in e for e in errors), errors)
    self.assertTrue(
        any("local workstation/temp path" in e for e in errors), errors)
    self.assertTrue(
        any("Unknown top-level JSON field" in e for e in errors), errors)

  def test_find_missing_triaging_doc_ids(self) -> None:
    all_ids = "\n".join([
        tr.HOTLIST_SECURITY_IMPACT_HEAD,
        tr.HOTLIST_SECURITY_IMPACT_BETA,
        tr.HOTLIST_SECURITY_IMPACT_STABLE,
        tr.HOTLIST_SECURITY_IMPACT_EXTENDED,
        tr.HOTLIST_SECURITY_IMPACT_NONE,
        tr.HOTLIST_V8_SANDBOX,
        tr.HOTLIST_V8_UNSUPPORTED_CHROME,
        tr.DEVTOOLS_COMPONENT_ID,
    ])
    self.assertEqual(tr.find_missing_triaging_doc_ids(all_ids), [])
    missing = tr.find_missing_triaging_doc_ids(
        all_ids.replace(tr.HOTLIST_V8_SANDBOX, ""))
    self.assertEqual(missing, [f"HOTLIST_V8_SANDBOX ({tr.HOTLIST_V8_SANDBOX})"])

  def test_env_session_id_auto_detection_and_placeholder_fallback(self) -> None:
    data = self._base_data()
    data["session_id"] = "<INVOKER_INFO_SESSION_ID>"
    with mock.patch.dict("os.environ",
                         {"ANTIGRAVITY_CONVERSATION_ID": "auto-conv-999"}):
      errors = tr.validate_triage_input(data, repo_root=self.temp_dir)
      self.assertEqual(errors, [])
      dec = tr.compute_triage_decision(data, repo_root=self.temp_dir)
      report = tr.render_triage_report(data, dec,
                                       tr.GitVersionResolver(self.temp_dir))
      self.assertIn("(Conversation ID: `auto-conv-999`)", report)

  def test_skill_md_preserves_single_source_of_truth_invariant(self) -> None:
    skill_md_path = SCRIPT_DIR.parent / "SKILL.md"
    self.assertTrue(skill_md_path.is_file(), f"Missing {skill_md_path}")
    skill_text = skill_md_path.read_text(encoding="utf-8")

    # 1. Zero references to legacy or removed triage skills
    self.assertNotIn("v8-poc-classification", skill_text)
    self.assertNotIn("v8-deterministic-security-triage", skill_text)
    self.assertLess(len(skill_text.splitlines()), 500)
    self.assertNotIn("\u2014", skill_text)

    # 2. Zero Symptom enum values in SKILL.md
    for symptom in tr.Symptom:
      self.assertNotIn(
          symptom.value,
          skill_text,
          f"SKILL.md must not hardcode symptom enum value {symptom.value!r}",
      )

    # 3. Zero policy/classification/severity/priority/hotlist tokens in SKILL.md
    word_boundary_tokens = (
        "S0",
        "S1",
        "S2",
        "S3",
        "P0",
        "P1",
        "P2",
        "P3",
        "Bug",
        "Vulnerability",
        "WAI",
    )
    for token in word_boundary_tokens:
      self.assertIsNone(
          re.search(rf"\b{re.escape(token)}\b", skill_text),
          f"SKILL.md must not leak policy/schema token {token!r}",
      )
    substring_tokens = (
        "Intended Behavior",
        "Failed to Reproduce",
        "```json",
        tr.HOTLIST_SECURITY_IMPACT_NONE,
        tr.HOTLIST_V8_SANDBOX,
        tr.HOTLIST_V8_UNSUPPORTED_CHROME,
        tr.CUSTOM_FIELD_RELEASE_BLOCK,
    )
    for token in substring_tokens:
      self.assertNotIn(
          token,
          skill_text,
          f"SKILL.md must not leak policy/schema token {token!r}",
      )

    # 4. Ensure command examples use the in-repo path (BAP capsule hook blocks
    #    `.gemini/config` paths and requires `--issue_id` on `issues` CLI).
    self.assertIn(
        "agents/skills/v8-security-triaging/scripts/triage_report.py",
        skill_text,
    )
    self.assertNotIn("~/.gemini/config/skills/", skill_text)
    self.assertIn(
        "download-attachment --issue_id=<issue_id>"
        " --attachment_id=<attachment_id> --output=tmp/<filename>",
        skill_text,
    )


if __name__ == "__main__":
  unittest.main()
