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

The default run uses small samples for presubmit. --exhaustive enables the
full Python coverage sweeps without a build; --d8 enables those too.
"""

import argparse
import collections
import contextlib
import inspect
import io
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
import harness
import run
from grammar import registry

# Set from the command line; None means the d8-dependent tests are skipped.
D8 = None
EXHAUSTIVE = False


def sample_size(n):
  return n if EXHAUSTIVE else min(n, 200)


def run_cases(cases):
  """Run |cases| through the harness, returning index -> result JSON string."""
  fd, path = tempfile.mkstemp(prefix="grammar_test_", suffix=".js")
  try:
    with os.fdopen(fd, "w", encoding="utf-8") as f:
      f.write(harness.emit_js(cases))
    p = subprocess.run([D8, path], capture_output=True, text=True, timeout=300)
  finally:
    os.unlink(path)
  return correctness_fuzzer.Runner._results(p.stdout)


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
    if not EXHAUSTIVE:
      self.skipTest("needs --exhaustive or --d8")
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
    if not EXHAUSTIVE:
      self.skipTest("needs --exhaustive or --d8")
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

  def test_profiles_keep_positive_rule_weights(self):
    for profile in grammar.PROFILES:
      weights = grammar.parse_weights(profile, None)
      ctx = grammar.Context(random.Random(1), False, False, 5, weights=weights)
      for rules in registry.GRAMMAR.values():
        for rule in rules:
          self.assertGreater(
              ctx.weight_of(rule), 0,
              "%s: %s.%s" % (profile, rule.prod, rule.name))

  def test_mode_specific_syntax_stays_in_its_mode(self):
    # The spec's [+UnicodeMode] / [+UnicodeSetsMode] parameters are what make
    # most syntax errors structurally impossible; a rule that loses its guard
    # would still generate, just invalidly.
    for pattern, flags, _, _ in generate(sample_size(4000)):
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
    for escape in (r"\0", r"\1", r"\12"):
      self.assertEqual(escape + "(?:)3", registry.concat_terms([escape, "3"]))
    self.assertEqual(r"\d3", registry.concat_terms([r"\d", "3"]))
    for pattern, _, _, _ in generate(sample_size(4000)):
      self.assertIsNone(re.search(r"\\[0-9]+[0-9]", pattern), pattern)
      for m in re.finditer(r"\(\?:\)", pattern):
        self.assertFalse(_in_class(pattern, m.start()), pattern)

  def test_generation_is_deterministic(self):
    # Findings are reported as a seed; a generator that drifted would make
    # every past finding irreproducible.
    self.assertEqual(generate(200, seed=7), generate(200, seed=7))

  def test_depth_budget_bounds_pattern_size(self):
    small = max(
        len(p) for p, _, _, _ in generate(sample_size(500), max_depth=2))
    large = max(
        len(p) for p, _, _, _ in generate(sample_size(500), max_depth=6))
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
    for _ in range(sample_size(3000)):
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
    for _, flags, _, _ in generate(sample_size(2000)):
      seen.update(flags)
    self.assertEqual(set("dgimsuvy"), seen)

  def test_u_and_v_are_never_combined(self):
    for _, flags, _, _ in generate(sample_size(3000)):
      self.assertFalse("u" in flags and "v" in flags, flags)

  def test_weight_override_shifts_the_distribution(self):

    def caret_share(weights):
      coverage = collections.Counter()
      generate(sample_size(1500), weights=weights, coverage=coverage)
      return coverage[("Assertion", "caret")]

    base = caret_share(None)
    boosted = caret_share(
        grammar.parse_weights("default", ["Assertion.caret=20"]))
    self.assertGreater(boosted, base)

  def test_class_string_disjunction_surrogates(self):
    # Inside `\q{...}`, lone surrogates only combine into a supplementary code
    # point when both halves are literals or `\uXXXX` escapes; any `\u{...}`
    # escape keeps them separate.  Verify that mixed escape forms and
    # literal/escaped combinations are reachable, and that every
    # ClassSetCharacter expansion records a single decoded character in
    # ctx.literals rather than escape sequences.
    weights = grammar.parse_weights("default", [
        "CharacterClass.negated_class=0",
        "ClassContents.empty_class=0",
        "ClassSetOperandOrRange.set_range=0",
        "ClassSetOperand.set_character=0",
        "ClassSetOperand.set_class_escape=0",
        "CharacterEscape.unicode_escape=10",
        "CharacterEscape.unicode_escape_braced=20",
        "ClassSetCharacter.literal=1",
        "ClassSetCharacter.escape=2",
    ])
    targets = {
        r"\ud83c\u{dca1}",
        r"\u{d83c}\udca1",
        r"\u{d83c}\u{dca1}",
        "\\ud83c\udca1",
        "\\u{d83c}\udca1",
        "\ud83c\\udca1",
        "\ud83c\\u{dca1}",
    }
    seen = set()
    rng = random.Random(13)
    for _ in range(8000):
      cov = collections.Counter()
      ctx = grammar.Context(rng, True, True, 1, weights=weights, coverage=cov)
      pat = grammar.expand(ctx, "CharacterClass")
      self.assertTrue(pat.startswith(r"[\q{") and pat.endswith("}]"), pat)
      n_chars = (
          cov[("ClassSetCharacter", "literal")] +
          cov[("ClassSetCharacter", "escape")])
      self.assertEqual(n_chars, len(ctx.literals))
      self.assertTrue(all(len(lit) == 1 for lit in ctx.literals), ctx.literals)
      for t in targets:
        if t in pat:
          seen.add(t)
          self.assertIn("\ud83c", ctx.literals)
          self.assertIn("\udca1", ctx.literals)
      if seen == targets:
        break
    self.assertEqual(targets, seen)


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

  def test_print_case_preserves_pattern_and_flags(self):

    class StubRunner:
      ref = test = None

      def run_one(self, config, pattern, flags, subject, last_index=0):
        return 0, ClassificationTest.OK

    for pattern, flags, expected in self.LITERAL_CASES:
      with self.subTest(pattern=pattern, flags=flags):
        raw = io.BytesIO()
        stream = io.TextIOWrapper(raw, encoding="utf-8")
        with contextlib.redirect_stdout(stream):
          correctness_fuzzer._print_case(StubRunner(), pattern, flags, pattern,
                                         0, "DIVERGENCE")
        lines = raw.getvalue().decode("utf-8").splitlines()
        self.assertEqual("  pattern: " + expected, lines[1])

  LITERAL_CASES = (
      (r"\d", "", r"/\d/"),
      (r"\\", "u", r"/\\/u"),
      ('"', "v", '/"/v'),
      ("/", "", r"/\//"),
      (r"\/", "u", r"/\//u"),
      (r"\\/", "v", r"/\\\//v"),
      ("[/]", "u", "/[/]/u"),
      (r"\[/", "", r"/\[\//"),
      ("", "v", "/(?:)/v"),
      ("\n\r\t\u2028\u2029\x00\x01\x7f", "",
       r"/\n\r\t\u2028\u2029\x00\x01\x7f/"),
      ("\\\n", "", r"/\n/"),
      ("\\\\\n", "u", r"/\\\n/u"),
      ("[\\\n/]", "", r"/[\n/]/"),
      ("\ud83c", "", r"/\ud83c/"),
      ("\udca1", "u", r"/\u{dca1}/u"),
      ("\ud83c", "v", r"/\u{d83c}/v"),
      ("\\\ud83c", "", r"/\ud83c/"),
      ("\\\x00", "", r"/\x00/"),
      ("\ud83c\udca1", "u", "/\U0001f0a1/u"),
      ("\U0001f0a1", "v", "/\U0001f0a1/v"),
      ("\ud83c" + r"\udca1", "u", r"/\u{d83c}\udca1/u"),
      (r"\ud83c" + "\udca1", "v", r"/\ud83c\u{dca1}/v"),
      ("[\ud83c" + r"\udca1]", "v", r"/[\u{d83c}\udca1]/v"),
      (r"[\q{" + "\ud83c" + r"\udca1}]", "v", r"/[\q{\u{d83c}\udca1}]/v"),
  )


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


class TestcaseFormatTest(unittest.TestCase):
  """Test emitted testcase files."""

  CASES = [
      ["a|b", "gi", "ab", 1],
      ["[\\p{L}--[a]]", "v", "\u00e9\U0001f600", 0],
      ["(?<n>x)\\k<n>", "d", "xx", 0],
  ]

  def test_emitted_script_is_standalone_ascii(self):
    text = harness.emit_js(self.CASES, header=["seed=1"])
    self.assertTrue(text.isascii())
    self.assertNotIn("arguments[", text)
    self.assertNotIn("read(", text)
    self.assertTrue(text.startswith("// seed=1\nconst cases = [\n"))

  def test_emitted_script_uses_regexp_foozzie_namespace(self):
    text = harness.emit_js(self.CASES)
    self.assertIn('v8-foozzie source: regexp-fuzzer:', text)
    self.assertNotIn('V8 correctness self-check failure', text)

  def test_one_case_per_line_with_trailing_commas(self):
    lines = harness.emit_js(self.CASES).splitlines()
    start = lines.index("const cases = [")
    end = lines.index("];")
    body = lines[start + 1:end]
    self.assertEqual(len(self.CASES), len(body))
    self.assertTrue(all(l.endswith(",") for l in body))

  def test_cases_round_trip(self):
    text = harness.emit_js(self.CASES)
    self.assertEqual(self.CASES, harness.parse_testcase(text))

  def test_parser_tolerates_a_minimized_file(self):
    lines = harness.emit_js(self.CASES).splitlines()
    del lines[2]
    lines[2] = lines[2].rstrip(",")
    lines[2] = '["ab", "y"]'
    cases = harness.parse_testcase("\n".join(lines))
    self.assertEqual([["a|b", "gi", "ab", 1], ["ab", "y", "", 0]], cases)

  def test_parser_rejects_a_foreign_file(self):
    with self.assertRaises(ValueError):
      harness.parse_testcase("print('not a fuzzer testcase');\n")

  def test_tag_keeps_shape_and_drops_literals(self):
    self.assertEqual(
        harness.source_tag("(ab|cd)+\\d", "gi"),
        harness.source_tag("(xy|zw)+\\d", "ig"))
    self.assertEqual(harness.source_tag("\\.", ""), "/")
    self.assertEqual(harness.source_tag("\\w\\1", ""), "/\\w\\1")
    self.assertLessEqual(len(harness.source_tag("(" * 500, "")), 41)


class FlagsFileTest(unittest.TestCase):
  """Test the foozzie flags emitted next to each testcase."""

  EXPERIMENTS = [[30, "jitless", "slow_path", "d8"],
                 [70, "jitless", "slow_path", "clang_x86/d8"]]
  ADDITIONAL = [[0.5, "--foo"], [0.5, "--bar --baz"]]

  def test_choose_flags_draws_from_the_tables(self):
    rng = random.Random(7)
    for _ in range(50):
      flags = run.choose_foozzie_flags(rng, self.EXPERIMENTS, self.ADDITIONAL,
                                       12345)
      self.assertEqual("--random-seed=12345", flags[0])
      self.assertEqual(["--first-config=jitless", "--second-config=slow_path"],
                       flags[1:3])
      self.assertIn(flags[3], ["--second-d8=d8", "--second-d8=clang_x86/d8"])
      extra = [f.split("=", 1)[1] for f in flags[4:]]
      self.assertIn(
          extra,
          [[], ["--foo"], ["--bar", "--baz"], ["--foo", "--bar", "--baz"]])

  def test_every_testcase_gets_a_flags_file(self):
    with tempfile.TemporaryDirectory() as out:
      run.main(
          ["--output_dir", out, "--no_of_files", "3", "--cases-per-file", "5"])
      for i in range(3):
        with open(os.path.join(out, "flags-%d.js" % i)) as f:
          flags = f.read().split()
        seeds = [flag for flag in flags if flag.startswith("--random-seed=")]
        self.assertEqual(1, len(seeds))
        self.assertIn(int(seeds[0].split("=", 1)[1]), range(1, 2**31))
        self.assertTrue(any(f.startswith("--first-config=") for f in flags))
        self.assertTrue(any(f.startswith("--second-d8=") for f in flags))


class GeneratedPatternTest(unittest.TestCase):
  """Properties that only a real engine can judge."""

  def setUp(self):
    # Checked here rather than with a class decorator: D8 is set by main()
    # after this module is imported, so a decorator would capture None.
    if D8 is None:
      self.skipTest("needs --d8")

  def test_printed_literals_preserve_matches(self):
    atoms = ("0", "d", "\\", "/", "[", "]", '"', "\n", "\r", "\t", "\u2028",
             "\u2029", "\x00", "\ud83c", "\udca1")
    subjects = [""] + list(atoms) + [a + b for a in atoms for b in atoms]
    cases = []
    patterns = [(p, f) for p, f, _ in ClassificationTest.LITERAL_CASES]
    for n in range(6):
      for c in ("/", "\n", "\r", "\u2028", "\u2029", "\x00", "\ud83c",
                "\ud83c\udca1"):
        for flags in ("", "u", "v", "gy", "du"):
          patterns.append(("\\" * n + c, flags))
    for pattern, flags in patterns:
      cases.append([
          pattern, flags,
          correctness_fuzzer._regexp_literal(pattern, flags), subjects, 0
      ])
    for pattern, flags, subject, last_index in generate(3000, seed=19):
      cases.append([
          pattern, flags,
          correctness_fuzzer._regexp_literal(pattern, flags), [subject],
          last_index
      ])
    script = "const cases = " + json.dumps(cases) + ";\n" + r"""
      function exec(r, subject, lastIndex) {
        r.lastIndex = lastIndex;
        const m = r.exec(subject);
        return JSON.stringify({r:m, idx:m?.index, g:m?.groups, di:m?.indices,
                               dg:m?.indices?.groups, li:r.lastIndex});
      }
      let checked = 0;
      for (const [pattern, flags, literal, subjects, lastIndex] of cases) {
        let original;
        try {
          original = new RegExp(pattern, flags);
        } catch {
          continue;  // Formatting is only defined for valid patterns.
        }
        const copy = eval(literal);
        if (copy.flags !== original.flags) throw new Error(literal);
        for (const subject of subjects) {
          const a = exec(original, subject, lastIndex);
          const b = exec(copy, subject, lastIndex);
          if (a !== b) {
            throw new Error(JSON.stringify({pattern, flags, literal, subject,
                                            lastIndex, a, b}));
          }
        }
        checked++;
      }
      print(checked);
    """
    with tempfile.TemporaryDirectory() as out:
      path = os.path.join(out, "literals.js")
      with open(path, "w", encoding="utf-8") as f:
        f.write(script)
      p = subprocess.run([D8, path], capture_output=True, text=True, timeout=60)
    self.assertEqual(0, p.returncode, p.stdout + p.stderr)
    self.assertGreaterEqual(int(p.stdout.strip()), 3000)

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

  def test_generated_testcase_runs_standalone(self):
    with tempfile.TemporaryDirectory() as out:
      run.main(
          ["--output_dir", out, "--no_of_files", "1", "--cases-per-file", "50"])
      path = os.path.join(out, "fuzz-0.js")
      p = subprocess.run([D8, path],
                         capture_output=True,
                         text=True,
                         timeout=300)
    self.assertEqual(0, p.returncode, p.stderr)
    lines = p.stdout.splitlines()
    self.assertEqual("DONE", lines[-1])
    self.assertEqual(50, len(correctness_fuzzer.Runner._results(p.stdout)))
    self.assertEqual(
        50,
        sum(1 for l in lines
            if l.startswith("v8-foozzie source: regexp-fuzzer:")))

  def test_testcase_replay_of_a_clean_file_finds_nothing(self):
    with tempfile.TemporaryDirectory() as out:
      run.main(
          ["--output_dir", out, "--no_of_files", "1", "--cases-per-file", "20"])
      p = subprocess.run([
          sys.executable,
          os.path.join(
              os.path.dirname(os.path.realpath(__file__)),
              "correctness_fuzzer.py"), "--ref", D8, "--test", D8, "--testcase",
          os.path.join(out, "fuzz-0.js")
      ],
                         capture_output=True,
                         text=True,
                         timeout=600)
    self.assertEqual(0, p.returncode, p.stdout + p.stderr)
    self.assertIn("20 case(s)", p.stdout)


def main():
  ap = argparse.ArgumentParser(description=__doc__)
  ap.add_argument("--d8", help="d8 binary; enables the execution tests")
  ap.add_argument(
      "--exhaustive",
      action="store_true",
      help="run full Python coverage sweeps and sample sizes")
  args, remaining = ap.parse_known_args()
  global D8, EXHAUSTIVE
  D8 = args.d8
  EXHAUSTIVE = args.exhaustive or D8 is not None
  if D8 is None:
    print("no --d8 given; skipping the tests that need one", file=sys.stderr)
  unittest.main(argv=[sys.argv[0]] + remaining)


if __name__ == "__main__":
  main()
