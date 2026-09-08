#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for the regexp fuzzer's pattern grammar.

The properties pinned here are the ones that decay silently: a generator
can keep producing patterns forever while emitting mostly syntax errors,
or while never reaching a production at all, and only a measurement
notices.  Both were true of the generator this grammar replaced.

The syntax-validity and match-rate tests need a d8 to judge what parses,
so they are skipped unless one is given:

  tools/regexp/correctness_fuzzer/grammar_test.py --d8 out/x64.release/d8

The rest run without a build.
"""

import argparse
import collections
import inspect
import json
import os
import random
import re
import subprocess
import sys
import tempfile
import unittest

import correctness_fuzzer
import grammar
from grammar import registry

HARNESS_PATH = os.path.join(
    os.path.dirname(os.path.realpath(__file__)), "harness.js")

# Set from the command line; None means the d8-dependent tests are skipped.
D8 = None


def run_cases(cases):
  """Run |cases| through the harness, returning index -> result JSON string."""
  fd, path = tempfile.mkstemp(prefix="grammar_test_", suffix=".json")
  try:
    with os.fdopen(fd, "w") as f:
      json.dump(cases, f)
    p = subprocess.run([D8, HARNESS_PATH, "--", path],
                       capture_output=True,
                       text=True,
                       timeout=300)
  finally:
    os.unlink(path)
  results = {}
  for line in p.stdout.splitlines():
    key, tab, value = line.partition("\t")
    if tab and key.isdigit():
      results[int(key)] = value
  return results


def generate(n, seed=1, **kwargs):
  rng = random.Random(seed)
  return [grammar.gen_case(rng, **kwargs) for _ in range(n)]


def _in_class(pattern, pos):
  """Is |pos| inside a character class?  Scans, since classes nest in v mode."""
  depth = i = 0
  while i < pos:
    if pattern[i] == "\\":
      i += 2
      continue
    if pattern[i] == "[":
      depth += 1
    elif pattern[i] == "]":
      depth = max(0, depth - 1)
    i += 1
  return depth > 0


class GrammarStructureTest(unittest.TestCase):
  """Properties of the grammar itself, checkable without a d8."""

  def test_every_rule_is_reachable(self):
    # A rule that never fires tests nothing, and no amount of case volume
    # reveals that on its own -- this is the check that caught a depth-budget
    # bug where every generated character class came out empty.
    coverage = collections.Counter()
    generate(3000, coverage=coverage)
    missing = [
        "%s.%s" % (prod, name)
        for prod, name in grammar.all_rules()
        if not coverage[(prod, name)]
    ]
    self.assertEqual([], missing, "unreachable at the default --max-depth")

  def test_every_profile_reaches_every_rule(self):
    # A profile reweights rules; setting one to a very low weight in effect
    # removes it, which should be a deliberate choice rather than a side
    # effect of tuning some other rule up.
    for profile in grammar.PROFILES:
      with self.subTest(profile=profile):
        coverage = collections.Counter()
        weights = grammar.parse_weights(profile, None)
        generate(3000, weights=weights, coverage=coverage)
        missing = [
            "%s.%s" % (prod, name)
            for prod, name in grammar.all_rules()
            if not coverage[(prod, name)]
        ]
        self.assertEqual([], missing)

  def test_mode_specific_syntax_stays_in_its_mode(self):
    # The spec's [+UnicodeMode] / [+UnicodeSetsMode] parameters are what make
    # most syntax errors structurally impossible; a rule that loses its guard
    # would still generate, just invalidly.
    for pattern, flags, _, _ in generate(4000):
      if "v" not in flags:
        self.assertNotIn(r"\q{", pattern)
        self.assertNotIn("&&", pattern)
        self.assertNotIn("--", pattern.replace(r"\-", ""))
      if "u" not in flags and "v" not in flags:
        self.assertNotIn(r"\p{", pattern)
        self.assertNotIn(r"\P{", pattern)
        self.assertNotIn(r"\u{", pattern)

  def test_digit_terminated_escapes_are_separated(self):
    # `\1` and `\0` are terminated by lookahead, so a digit right after either
    # silently reparses it.  The `(?:)` separator that prevents this is only a
    # separator between Terms; inside a class it would be four more members.
    for pattern, _, _, _ in generate(4000):
      self.assertIsNone(re.search(r"\\[0-9]+[0-9]", pattern), pattern)
      for m in re.finditer(r"\(\?:\)", pattern):
        self.assertFalse(_in_class(pattern, m.start()), pattern)

  def test_generation_is_deterministic(self):
    # Findings are reported as a seed; a generator that drifted would make
    # every past finding irreproducible.
    self.assertEqual(generate(200, seed=7), generate(200, seed=7))

  def test_depth_budget_bounds_pattern_size(self):
    small = max(len(p) for p, _, _, _ in generate(500, max_depth=2))
    large = max(len(p) for p, _, _, _ in generate(500, max_depth=6))
    self.assertLess(small, large)

  def test_unknown_profile_and_rule_are_rejected(self):
    with self.assertRaises(ValueError):
      grammar.parse_weights("no-such-profile", None)
    with self.assertRaises(ValueError):
      grammar.parse_weights("default", ["No.Such.Rule=2"])
    with self.assertRaises(ValueError):
      grammar.parse_weights("default", ["missing-equals-sign"])

  def test_unknown_production_is_reported(self):
    # A mistyped production used to surface as an UnboundLocalError from
    # inside expand(), with the offending name nowhere in the traceback.
    ctx = grammar.Context(random.Random(1), False, False, 5)
    with self.assertRaises(grammar.GrammarError) as e:
      grammar.expand(ctx, "Disjuncton")
    self.assertIn("no such production", str(e.exception))
    self.assertIn("Disjuncton", str(e.exception))
    # The registry was a defaultdict, so merely looking a typo up created it;
    # a later real rule would then have joined the phantom production instead
    # of the intended one.
    self.assertNotIn("Disjuncton", registry.GRAMMAR)

  def test_production_with_no_eligible_alternative_is_reported(self):
    # Distinct from a typo: the production exists, but every alternative is
    # guarded off for these parameters, which is a live derivation dead end
    # rather than a misspelling.
    ctx = grammar.Context(random.Random(1), False, False, 5)

    @registry.rule("OnlyGuardedOff", guard=lambda c: c.unicode_mode)
    def _unreachable(ctx):
      return ""

    try:
      with self.assertRaises(grammar.GrammarError) as e:
        grammar.expand(ctx, "OnlyGuardedOff")
      self.assertIn("guarded off", str(e.exception))
    finally:
      del registry.GRAMMAR["OnlyGuardedOff"]

  def test_duplicate_rule_names_are_rejected(self):
    # Rules are addressed as "Production.rule" by --weight and the coverage
    # report, so a duplicate would make one of the two unreachable.
    @registry.rule("DuplicateProbe")
    def probe(ctx):
      return ""

    try:
      with self.assertRaises(grammar.GrammarError):

        @registry.rule("DuplicateProbe")
        def probe(ctx):  # noqa: F811  (the point is the redefinition)
          return ""
    finally:
      del registry.GRAMMAR["DuplicateProbe"]

  def test_rules_make_no_hidden_weighted_choices(self):
    # Every choice between meaningfully different outputs belongs to a rule
    # with its own weight; an rng call inside a rule body would be a weight
    # that neither --weight nor --coverage can reach.  Drawing a character
    # from a pool is not such a choice, so `choice` is allowed and the
    # branching primitives are not.
    src = inspect.getsource(sys.modules["grammar.rules"])
    for lineno, line in enumerate(src.splitlines(), 1):
      code = line.split("#")[0]
      self.assertNotIn("rng.random()", code, "grammar/rules.py:%d" % lineno)

  def test_quantifier_bounds_reach_past_the_unroll_threshold(self):
    # RegExpQuantifier unrolls a body when min is in 1..kMaxUnrolledMinMatches
    # (3, regexp-compiler-tonode.cc), recursing on the remainder -- so {2,5}
    # unrolls entirely and "some bound exceeds 3" is not the property that
    # matters.  A min above the threshold is what forces the counter-based
    # loop; {0,} is excluded deliberately, since it is only `*` spelled the
    # long way and reaches no node shape the bare quantifiers miss.
    #
    # Counted off the QuantifierPrefix rules rather than off the pattern text,
    # which cannot tell a quantifier from a `\q{9}` or a `{` inside a class.
    counted = looping = 0
    rng = random.Random(1)
    for _ in range(3000):
      ctx = grammar.Context(rng, False, False, 5)
      m = re.fullmatch(r"\{(\d+),?\d*\}",
                       grammar.expand(ctx, "QuantifierPrefix"))
      if m:
        counted += 1
        if int(m.group(1)) > 3:
          looping += 1
    self.assertTrue(counted, "no counted quantifiers generated at all")
    self.assertGreater(
        looping, 0, "every counted quantifier min stays "
        "within the unroll threshold")

  def test_flags_cover_the_result_shaping_ones(self):
    # `g` drives the global lastIndex path (distinct from sticky) and `d`
    # adds match indices, a whole output surface; neither was generated
    # before.
    seen = set()
    for _, flags, _, _ in generate(2000):
      seen.update(flags)
    self.assertEqual(set("dgimsuvy"), seen)

  def test_u_and_v_are_never_combined(self):
    for _, flags, _, _ in generate(3000):
      self.assertFalse("u" in flags and "v" in flags, flags)

  def test_weight_override_shifts_the_distribution(self):

    def caret_share(weights):
      coverage = collections.Counter()
      generate(1500, weights=weights, coverage=coverage)
      return coverage[("Assertion", "caret")]

    base = caret_share(None)
    boosted = caret_share(
        grammar.parse_weights("default", ["Assertion.caret=20"]))
    self.assertGreater(boosted, base)


class ClassificationTest(unittest.TestCase):
  """How a pair of harness results is turned into a finding.

  The batch scan and the single-case path share this, so a disagreement
  between them shows up as a finding that cannot be reproduced or minimized.
  """

  OK = '{"cold":{"r":["a"],"idx":0,"li":0},"warm":{"r":["a"],"idx":0,"li":0}}'
  OTHER = '{"cold":{"r":["z"],"idx":0,"li":0},"warm":{"r":["z"],"idx":0,"li":0}}'
  SELF_DIFF = ('{"cold":{"r":["a"],"idx":0,"li":0},'
               '"warm":{"r":["b"],"idx":0,"li":0}}')

  def test_identical_clean_results_are_not_a_finding(self):
    self.assertIsNone(correctness_fuzzer.classify(0, self.OK, 0, self.OK))

  def test_test_disagreeing_with_reference_is_a_divergence(self):
    self.assertEqual("DIVERGENCE",
                     correctness_fuzzer.classify(0, self.OK, 0, self.OTHER))

  def test_a_configuration_disagreeing_with_itself_is_a_finding(self):
    # Needs no reference: the two execs are the same regexp over the same
    # subject, so whichever side does this is wrong on its own.
    for ref, test in ((self.SELF_DIFF, self.OK), (self.OK, self.SELF_DIFF)):
      self.assertEqual("COLD/WARM",
                       correctness_fuzzer.classify(0, ref, 0, test))

  def test_a_reference_error_establishes_no_ground_truth(self):
    # Syntax the reference rejects, and an exec that raised (a stack or
    # backtrack limit), are both configuration-dependent rather than wrong
    # answers -- reporting them would bury real findings.
    for ref in (
        '"ERR_CTOR"',
        '{"cold":"ERR_EXEC:SyntaxError","warm":"ERR_EXEC:SyntaxError"}'):
      self.assertIsNone(correctness_fuzzer.classify(0, ref, 0, self.OK))

  def test_a_hard_abort_is_a_finding_regardless(self):
    self.assertEqual("CRASH",
                     correctness_fuzzer.classify(0, self.OK, 1, self.OK))

  def test_a_timeout_establishes_no_ground_truth(self):
    self.assertIsNone(
        correctness_fuzzer.classify(correctness_fuzzer.TIMEOUT_RC, None, 0,
                                    self.OK))

  def test_a_missing_result_line_is_a_divergence(self):
    # One side crashed or timed out partway and never emitted this case.
    self.assertEqual("DIVERGENCE",
                     correctness_fuzzer.classify(0, self.OK, 0, None))


class MinimizerTest(unittest.TestCase):
  """Shrinking, which has to keep up with the large quantifier bounds."""

  def test_large_bounds_shrink_in_log_steps(self):
    # Decrementing a bound in the hundreds costs one run of both
    # configurations per unit, which dominates the whole minimization.
    self.assertEqual([0, 1, 250, 375, 499],
                     list(correctness_fuzzer._shrink_targets(500)))
    self.assertEqual([0], list(correctness_fuzzer._shrink_targets(1)))
    self.assertEqual([], list(correctness_fuzzer._shrink_targets(0)))

  def test_quantifier_forms_round_trip(self):
    self.assertEqual("{5}", correctness_fuzzer._quantifier(5, False, None))
    self.assertEqual("{5,}", correctness_fuzzer._quantifier(5, True, None))
    self.assertEqual("{5,9}", correctness_fuzzer._quantifier(5, True, 9))

  def test_minimization_converges_on_a_large_bound(self):

    class StubRunner:
      """Reports a finding while any quantifier min is still >= 3."""

      def __init__(self):
        self.calls = 0

      def finding(self, pattern, flags, subject, last_index=0):
        self.calls += 1
        return any(
            int(m.group(1)) >= 3
            for m in re.finditer(r"\{(\d+),?\d*\}", pattern))

    runner = StubRunner()
    pattern, _, _, _ = correctness_fuzzer.minimize(runner, "a{200,900}b", "",
                                                   "ab")
    # 3 is the smallest bound this stub still reproduces at, so the shrink has
    # to reach it exactly -- stopping early would leave a repro carrying a
    # bound that has nothing to do with the finding.  The literals go too,
    # since this stub looks at nothing else.
    self.assertEqual("{3}", pattern)
    # A decrementing shrink would need hundreds of runs to get here.
    self.assertLess(runner.calls, 100)


class GeneratedPatternTest(unittest.TestCase):
  """Properties that only a real engine can judge."""

  def setUp(self):
    # Checked here rather than with a class decorator: D8 is set by main()
    # after this module is imported, so a decorator would capture None.
    if D8 is None:
      self.skipTest("needs --d8")

  def test_patterns_are_syntactically_valid(self):
    # The generator this replaced emitted 44% syntax errors, so nearly half of
    # every run was discarded before executing anything.  Zero is the right bar
    # rather than a small percentage: every error found so far came from a
    # context-sensitivity the grammar has to model (a lookahead constraint, a
    # static-semantics rule), and each showed up in well under 0.1% of cases --
    # a tolerance wide enough to absorb one is wide enough to hide it.
    for seed in (11, 13, 17):
      with self.subTest(seed=seed):
        cases = generate(4000, seed=seed)
        results = run_cases(cases)
        bad = [cases[i][:2] for i, r in results.items() if r == '"ERR_CTOR"']
        self.assertEqual([], bad[:10], "syntax errors")

  def test_a_meaningful_share_of_cases_match(self):
    # Subjects are drawn from the characters the pattern mentions.  Without
    # that, cases fail at the first character and only the reject path is
    # tested; the previous generator matched on 21% of cases, and 64% of those
    # were empty-string matches.
    cases = generate(4000, seed=13)
    results = run_cases(cases)
    matched = [
        r for r in results.values()
        if r != '"ERR_CTOR"' and json.loads(r).get("warm") is not None
    ]
    self.assertGreater(len(matched) / len(cases), 0.3)
    nonempty = [r for r in matched if json.loads(r)["warm"]["r"][0] != ""]
    self.assertGreater(len(nonempty) / len(matched), 0.3)

  def test_the_two_execs_agree(self):
    # The harness reports a cold and a warm exec of every case and the driver
    # treats a mismatch as a finding, so a clean engine has to produce zero of
    # them -- otherwise the check is noise rather than a signal.
    cases = generate(4000, seed=19)
    disagreed = [
        cases[i]
        for i, r in run_cases(cases).items()
        if r != '"ERR_CTOR"' and json.loads(r)["cold"] != json.loads(r)["warm"]
    ]
    self.assertEqual([], disagreed[:10])

  def test_harness_reports_lastindex(self):
    # lastIndex selects where a sticky attempt starts; it was never varied
    # before, which is half of sticky semantics untested.
    results = run_cases([["a", "y", "ba", 1], ["a", "y", "ba", 0]])
    self.assertEqual(2, json.loads(results[0])["warm"]["li"])
    self.assertIsNone(json.loads(results[1])["warm"])

  def test_harness_reports_indices_and_groups(self):
    # `d` and `(?<name>)` add result surfaces built by their own code paths.
    results = run_cases([["(?<g>a)(b)", "d", "ab", 0], ["a", "", "a", 0]])
    warm = json.loads(results[0])["warm"]
    self.assertEqual([[0, 2], [0, 1], [1, 2]], warm["di"])
    self.assertEqual({"g": [0, 1]}, warm["dg"])
    self.assertEqual({"g": "a"}, warm["g"])
    # Absent without the flag or the construct, rather than reported as null.
    self.assertNotIn("di", json.loads(results[1])["warm"])


def main():
  ap = argparse.ArgumentParser(description=__doc__)
  ap.add_argument("--d8", help="d8 binary; enables the execution tests")
  args, remaining = ap.parse_known_args()
  global D8
  D8 = args.d8
  if D8 is None:
    print("no --d8 given; skipping the tests that need one", file=sys.stderr)
  unittest.main(argv=[sys.argv[0]] + remaining)


if __name__ == "__main__":
  main()
