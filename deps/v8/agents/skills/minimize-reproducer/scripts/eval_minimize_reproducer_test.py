#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for eval_minimize_reproducer.py."""

import json
import os
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(__file__))
import eval_minimize_reproducer


class EvalMinimizeReproducerTest(unittest.TestCase):

  def setUp(self):
    self.tmpdir = tempfile.TemporaryDirectory()

  def tearDown(self):
    self.tmpdir.cleanup()

  def test_parse_transcript_metrics_counts_turns_and_subagents(self):
    tpath = os.path.join(self.tmpdir.name, "transcript.jsonl")
    steps = [
        {
            "type": "USER_INPUT",
            "created_at": "2026-11-09T10:00:00Z",
        },
        {
            "type":
                "PLANNER_RESPONSE",
            "created_at":
                "2026-11-09T10:00:02Z",
            "tool_calls": [
                {
                    "name": "invoke_subagent",
                    "arguments": {
                        "TypeName": "self",
                        "Model": "inherit"
                    },
                },
                {
                    "name": "invoke_subagent",
                    "arguments": {
                        "TypeName": "self",
                        "Model": "flash"
                    },
                },
                {
                    "name": "run_command",
                    "arguments": {
                        "CommandLine":
                            "ls -la ~/.gemini/jetski/brain/123/scratch"
                    },
                },
            ],
        },
        {
            "type":
                "PLANNER_RESPONSE",
            "created_at":
                "2026-11-09T10:00:10Z",
            "tool_calls": [{
                "name": "run_command",
                "arguments": {
                    "CommandLine": "git worktree list"
                },
            }],
        },
    ]
    with open(tpath, "w", encoding="utf-8") as f:
      for s in steps:
        f.write(json.dumps(s) + "\n")

    m = eval_minimize_reproducer.parse_transcript_metrics(tpath)
    self.assertEqual(m["planner_turns"], 2)
    self.assertEqual(m["tool_calls_total"], 4)
    self.assertEqual(m["subagent_spawns_total"], 2)
    self.assertEqual(m["heavy_subagent_spawns"], 1)
    self.assertEqual(m["polling_calls"], 1)
    self.assertEqual(m["wall_seconds"], 10.0)

  def test_grade_case_minimize_dd_passes_and_fails_cleanly(self):
    wt = os.path.join(self.tmpdir.name, "wt")
    tmp_dir = os.path.join(wt, "tmp")
    os.makedirs(tmp_dir)
    d8_sh = os.path.join(tmp_dir, "fake_d8.sh")
    with open(d8_sh, "w", encoding="utf-8") as f:
      f.write(eval_minimize_reproducer.FAKE_D8_SH)
    os.chmod(d8_sh, 0o755)
    poc_min = os.path.join(tmp_dir, "poc-min.js")
    with open(poc_min, "w", encoding="utf-8") as f:
      f.write("function TRIGGER_DCHECK() {}\nTRIGGER_DCHECK();\n")

    metrics_after = {
        "planner_turns": 12,
        "tool_calls_total": 13,
        "subagent_spawns_total": 0,
        "heavy_subagent_spawns": 0,
        "polling_calls": 0,
        "wall_seconds": 70.0,
        "command_lines": ["vpython3 agents/.../minimize_dd.py ..."],
        "tools_used": ["run_command"],
    }
    res_after = eval_minimize_reproducer.grade_case("minimize_dd", wt,
                                                    metrics_after,
                                                    "--expose-gc")
    self.assertTrue(res_after["passed"], res_after)
    self.assertTrue(res_after["used_minimize_dd"])
    self.assertNotIn("used_minimize_dd", res_after["checks"])

    metrics_before = {
        "planner_turns": 22,
        "tool_calls_total": 25,
        "subagent_spawns_total": 0,
        "heavy_subagent_spawns": 0,
        "polling_calls": 0,
        "wall_seconds": 140.0,
        "command_lines": ["bash tmp/fake_d8.sh tmp/poc.js"],
        "tools_used": ["run_command"],
    }
    res_before = eval_minimize_reproducer.grade_case("minimize_dd", wt,
                                                     metrics_before,
                                                     "--expose-gc")
    # Clean Before outputs still pass outcome grading; tool adoption is tracked separately.
    self.assertTrue(res_before["passed"], res_before)
    self.assertFalse(res_before["used_minimize_dd"])
    self.assertNotIn("efficient_turns", res_before["checks"])

    with open(poc_min, "w", encoding="utf-8") as f:
      f.write("// bloated\n" * 6 +
              "function TRIGGER_DCHECK() {}\nTRIGGER_DCHECK();\n")
    res_bloated = eval_minimize_reproducer.grade_case("minimize_dd", wt,
                                                      metrics_before,
                                                      "--expose-gc")
    self.assertFalse(res_bloated["passed"])
    self.assertFalse(res_bloated["checks"]["minimal_lines"])

  def test_build_semantic_poc_reproduces_and_checks_clean_output(self):
    src = eval_minimize_reproducer.build_semantic_poc_js()
    self.assertIn("class __FuzzerEnv", src)
    self.assertIn("const __noise_50", src)

    wt = os.path.join(self.tmpdir.name, "wt3")
    tmp_dir = os.path.join(wt, "tmp")
    os.makedirs(tmp_dir)
    d8_sh = os.path.join(tmp_dir, "fake_d8_semantic.sh")
    with open(d8_sh, "w", encoding="utf-8") as f:
      f.write("#!/usr/bin/env bash\n"
              "if grep -q 'triggerTarget' \"$2\"; then\n"
              "  echo 'Fatal error: DCHECK failed: target_invariant()'\n"
              "  exit 134\n"
              "fi\n")
    os.chmod(d8_sh, 0o755)

    poc_min = os.path.join(tmp_dir, "poc-min.js")
    metrics = {
        "planner_turns": 10,
        "tool_calls_total": 10,
        "subagent_spawns_total": 0,
        "heavy_subagent_spawns": 0,
        "polling_calls": 0,
        "wall_seconds": 40.0,
        "command_lines": ["vpython3 agents/.../minimize_dd.py ..."],
        "tools_used": ["run_command"],
    }

    # Mechanical ddmin alone leaves wrapper class/loop -> fails semantic check.
    with open(poc_min, "w", encoding="utf-8") as f:
      f.write("class __FuzzerEnv {}\ngc();\ntriggerTarget(101);\n")
    res_unclean = eval_minimize_reproducer.grade_case("semantic_minimize", wt,
                                                      metrics, "")
    self.assertFalse(res_unclean["checks"]["smart_semantic_simplification"])

    # Smart semantic rewrite -> 2 clean lines -> passes.
    with open(poc_min, "w", encoding="utf-8") as f:
      f.write("gc();\ntriggerTarget(101);\n")
    res_clean = eval_minimize_reproducer.grade_case("semantic_minimize", wt,
                                                    metrics, "")
    self.assertTrue(res_clean["passed"], res_clean)
    self.assertNotIn("efficient_turns", res_clean["checks"])

  def test_historic_582176947_fixture_and_grader(self):
    src = eval_minimize_reproducer.build_582176947_poc_js()
    self.assertIn("Africa/Monrovia", src)
    self.assertIn("Uint32Array", src)

    wt = os.path.join(self.tmpdir.name, "wt4")
    tmp_dir = os.path.join(wt, "tmp")
    os.makedirs(tmp_dir)
    d8_bin = os.path.join(eval_minimize_reproducer.DEFAULT_REPO_ROOT, "out",
                          "x64.release", "d8")
    repro_sh = os.path.join(tmp_dir, "repro.sh")
    with open(repro_sh, "w", encoding="utf-8") as f:
      f.write(eval_minimize_reproducer.build_582176947_d8_sh(d8_bin))
    os.chmod(repro_sh, 0o755)

    metrics = {
        "planner_turns": 25,
        "tool_calls_total": 26,
        "subagent_spawns_total": 0,
        "heavy_subagent_spawns": 0,
        "polling_calls": 0,
        "wall_seconds": 120.0,
        "command_lines": ["vpython3 agents/.../minimize_dd.py ..."],
        "tools_used": ["run_command"],
    }
    poc_min = os.path.join(tmp_dir, "poc-min.js")

    # Unsimplified fuzzer construct still fails clean_regression_test.
    with open(poc_min, "w", encoding="utf-8") as f:
      f.write(src)
    res_unclean = eval_minimize_reproducer.grade_case("historic_582176947", wt,
                                                      metrics, "")
    self.assertFalse(res_unclean["checks"]["clean_regression_test"])

    # Clean regression test modeled on test/mjsunit/maglev/regress-582176947.js.
    with open(poc_min, "w", encoding="utf-8") as f:
      f.write("async function foo(cond) {\n"
              "  while (cond) {\n"
              "    while (cond) { await 1; }\n"
              "  }\n"
              "}\n"
              "%PrepareFunctionForOptimization(foo);\n"
              "foo(false);\n"
              '%OptimizeFunctionOnNextCall(foo, "concurrent");\n'
              "foo(false);\n"
              "%FinalizeOptimization();\n")
    res_clean = eval_minimize_reproducer.grade_case("historic_582176947", wt,
                                                    metrics, "")
    if os.path.exists(d8_bin):
      self.assertTrue(res_clean["passed"], res_clean)
    else:
      self.assertTrue(res_clean["checks"]["clean_regression_test"])

  def test_historic_577269348_fixture_and_grader(self):
    src = eval_minimize_reproducer.build_577269348_poc_js()
    self.assertIn("__unused_realm", src)
    self.assertIn("__deadFuzzHelper", src)

    wt = os.path.join(self.tmpdir.name, "wt5")
    tmp_dir = os.path.join(wt, "tmp")
    os.makedirs(tmp_dir)
    d8_bin = os.path.join(eval_minimize_reproducer.DEFAULT_REPO_ROOT, "out",
                          "x64.release", "d8")
    repro_sh = os.path.join(tmp_dir, "repro.sh")
    with open(repro_sh, "w", encoding="utf-8") as f:
      f.write(eval_minimize_reproducer.build_577269348_d8_sh(d8_bin))
    os.chmod(repro_sh, 0o755)

    metrics = {
        "planner_turns": 25,
        "tool_calls_total": 26,
        "subagent_spawns_total": 0,
        "heavy_subagent_spawns": 0,
        "polling_calls": 0,
        "wall_seconds": 120.0,
        "command_lines": ["vpython3 agents/.../minimize_dd.py ..."],
        "tools_used": ["run_command"],
    }
    poc_min = os.path.join(tmp_dir, "poc-min.js")

    with open(poc_min, "w", encoding="utf-8") as f:
      f.write(src)
    res_unclean = eval_minimize_reproducer.grade_case("historic_577269348", wt,
                                                      metrics, "")
    self.assertFalse(res_unclean["checks"]["clean_regression_test"])

    # Clean regression test modeled on test/mjsunit/compiler/regress-577269348.js.
    with open(poc_min, "w", encoding="utf-8") as f:
      f.write("const realm = Realm.current();\n"
              'const code = "(function* gen() { yield 42; })()";\n'
              "const gen = Realm.eval(realm, code);\n"
              "gen.next();\n"
              "gc();\n"
              "console.profile();\n"
              "Realm.eval(realm, code);\n")
    res_clean = eval_minimize_reproducer.grade_case("historic_577269348", wt,
                                                    metrics, "")
    if os.path.exists(d8_bin):
      self.assertTrue(res_clean["passed"], res_clean)
    else:
      self.assertTrue(res_clean["checks"]["clean_regression_test"])

  def test_historic_578950019_fixture_and_grader(self):
    wt = os.path.join(self.tmpdir.name, "wt6")
    os.makedirs(os.path.join(wt, "tmp"))
    d8_bin = os.path.join(eval_minimize_reproducer.DEFAULT_REPO_ROOT, "out",
                          "x64.release", "d8")
    repro = os.path.join(wt, "tmp", "repro.sh")
    with open(repro, "w", encoding="utf-8") as f:
      f.write(eval_minimize_reproducer.build_578950019_d8_sh(d8_bin))
    os.chmod(repro, 0o755)
    poc_min = os.path.join(wt, "tmp", "poc-min.js")
    with open(poc_min, "w", encoding="utf-8") as f:
      f.write(eval_minimize_reproducer.build_578950019_poc_js())
    metrics = {
        "command_lines": ["vpython3 minimize_dd.py"],
        "subagent_spawns_total": 0,
    }
    res_bloated = eval_minimize_reproducer.grade_case("historic_578950019", wt,
                                                      metrics, "")
    if os.path.exists(d8_bin):
      self.assertTrue(res_bloated["checks"]["min_file_reproduces"], res_bloated)
    self.assertFalse(res_bloated["checks"]["clean_regression_test"])
    with open(poc_min, "w", encoding="utf-8") as f:
      f.write("function inspect() { return bar.arguments; }\n"
              "function bar(arr, trigger) {\n"
              "  if (trigger) { arr.shift(); gc(); inspect(); }\n"
              "}\n"
              "function foo(trigger) {\n"
              "  let arr = new Array(102).fill(1);\n"
              "  bar(arr, trigger);\n"
              "}\n"
              "%PrepareFunctionForOptimization(foo);\n"
              "foo(false);\n"
              "%OptimizeFunctionOnNextCall(foo);\n"
              "foo(true);\n")
    res_clean = eval_minimize_reproducer.grade_case("historic_578950019", wt,
                                                    metrics, "")
    if os.path.exists(d8_bin):
      self.assertTrue(res_clean["passed"], res_clean)
    else:
      self.assertTrue(res_clean["checks"]["clean_regression_test"])

  def test_historic_574021176_fixture_and_grader(self):
    wt = os.path.join(self.tmpdir.name, "wt7")
    os.makedirs(os.path.join(wt, "tmp"))
    d8_bin = os.path.join(eval_minimize_reproducer.DEFAULT_REPO_ROOT, "out",
                          "x64.release", "d8")
    repro = os.path.join(wt, "tmp", "repro.sh")
    with open(repro, "w", encoding="utf-8") as f:
      f.write(eval_minimize_reproducer.build_574021176_d8_sh(d8_bin))
    os.chmod(repro, 0o755)
    poc_min = os.path.join(wt, "tmp", "poc-min.js")
    with open(poc_min, "w", encoding="utf-8") as f:
      f.write(eval_minimize_reproducer.build_574021176_poc_js())
    metrics = {
        "command_lines": ["vpython3 minimize_dd.py"],
        "subagent_spawns_total": 0,
    }
    res_bloated = eval_minimize_reproducer.grade_case("historic_574021176", wt,
                                                      metrics, "")
    if os.path.exists(d8_bin):
      self.assertTrue(res_bloated["checks"]["min_file_reproduces"], res_bloated)
    self.assertFalse(res_bloated["checks"]["clean_regression_test"])
    with open(poc_min, "w", encoding="utf-8") as f:
      f.write(
          "// Trigger class heritage throw inside a generator.\n"
          "class Base {}\n"
          "function* trigger(x, gate, holder) {\n"
          "  const capture = () => x;\n"
          "  class Sub extends (gate(), holder.q) {}\n"
          "  yield capture;\n"
          "}\n"
          "function gateThrow() { throw new Error(); }\n"
          "%PrepareFunctionForOptimization(trigger);\n"
          "try { trigger(0, gateThrow, { q: Base }).next(); } catch (e) {}\n"
          "%OptimizeFunctionOnNextCall(trigger);\n"
          "trigger(1, () => {}, { q: Base }).next();\n")
    res_clean = eval_minimize_reproducer.grade_case("historic_574021176", wt,
                                                    metrics, "")
    if os.path.exists(d8_bin):
      self.assertTrue(res_clean["passed"], res_clean)
    else:
      self.assertTrue(res_clean["checks"]["clean_regression_test"])

  def test_strip_eval_leaks_removes_grader_prompts_and_regress_tests(self):
    wt = os.path.join(self.tmpdir.name, "wt_strip")
    grader = os.path.join(wt, "agents", "skills", "minimize-reproducer",
                          "scripts", "eval_minimize_reproducer.py")
    prompt_dir = os.path.join(wt, "agents", "prompts", "eval",
                              "minimize-reproducer")
    dot_grader = os.path.join(wt, ".agents", "skills", "minimize-reproducer",
                              "scripts", "eval_minimize_reproducer.py")
    keep_dd = os.path.join(wt, "agents", "skills", "minimize-reproducer",
                           "scripts", "minimize_dd.py")
    reg_test = os.path.join(wt, "test", "mjsunit", "maglev",
                            "regress-574021176.js")
    keep_file = os.path.join(wt, "README.md")
    for p in (grader, dot_grader, keep_dd, reg_test, keep_file):
      os.makedirs(os.path.dirname(p), exist_ok=True)
      with open(p, "w", encoding="utf-8") as f:
        f.write("// test\n")
    os.makedirs(prompt_dir, exist_ok=True)
    with open(
        os.path.join(prompt_dir, "historic_574021176_prompt.md"),
        "w",
        encoding="utf-8",
    ) as f:
      f.write("prompt\n")

    eval_minimize_reproducer._run(["git", "init", "-q"], cwd=wt)
    eval_minimize_reproducer._run(
        ["git", "config", "user.email", "test@example.com"], cwd=wt)
    eval_minimize_reproducer._run(["git", "config", "user.name", "Test"],
                                  cwd=wt)
    eval_minimize_reproducer._run(["git", "add", "-A"], cwd=wt)
    eval_minimize_reproducer._run(["git", "commit", "-q", "-m", "init"], cwd=wt)

    eval_minimize_reproducer._strip_eval_leaks(wt)
    eval_minimize_reproducer._commit_clean_eval_snapshot(wt)
    self.assertFalse(os.path.exists(grader))
    self.assertFalse(os.path.exists(dot_grader))
    self.assertFalse(os.path.exists(prompt_dir))
    self.assertFalse(os.path.exists(reg_test))
    self.assertTrue(os.path.exists(keep_dd))
    self.assertEqual(
        eval_minimize_reproducer._run(["git", "status", "--short"],
                                      cwd=wt).stdout.strip(),
        "",
    )
    self.assertNotEqual(
        eval_minimize_reproducer._run(
            [
                "git", "cat-file", "-e",
                "HEAD:test/mjsunit/maglev/regress-574021176.js"
            ],
            cwd=wt,
            check=False,
        ).returncode,
        0,
    )

  def test_extract_response_text_includes_send_message(self):
    tpath = os.path.join(self.tmpdir.name, "subagent_transcript.jsonl")
    steps = [
        {
            "type":
                "PLANNER_RESPONSE",
            "tool_calls": [{
                "name": "send_message",
                "arguments": {
                    "Recipient": "parent",
                    "Message": "Found sibling branch eval-crbug-99990001.",
                },
            }],
        },
        {
            "type": "PLANNER_RESPONSE",
            "content": "Reported findings to parent.",
        },
    ]
    with open(tpath, "w", encoding="utf-8") as f:
      for s in steps:
        f.write(json.dumps(s) + "\n")

    extracted = eval_minimize_reproducer.extract_response_text(tpath)
    self.assertIn("eval-crbug-99990001", extracted)
    self.assertIn("Reported findings to parent.", extracted)


if __name__ == "__main__":
  unittest.main()
