#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Differential correctness fuzzer for the regexp engine.

Generates random patterns / flags / subjects, runs them through two d8
configurations, and reports any case where the observed result (match,
captures, index, lastIndex) differs -- or where one configuration
crashes.  Each found divergence is delta-minimized before printing.

A two-config diff catches both wrong-answer miscompiles and crashes
that a single run would miss, exercising regexp shapes that mjsunit and
the recorded benchmark corpus do not cover exhaustively.  Use it to
compare any two configurations: a build under test against a stock
reference, the interpreter against the JIT, or two flag settings.

Typical use -- a build under test against a stock reference build:

  tools/regexp/correctness_fuzzer/correctness_fuzzer.py \
      --ref   ~/stock/out/x64.release/d8 \
      --test  out/x64.release/d8

Or compare two flag configurations of one binary (no second build):

  tools/regexp/correctness_fuzzer/correctness_fuzzer.py \
      --ref   "out/x64.release/d8:--regexp-interpret-all" \
      --test  "out/x64.release/d8"

Reproduce / minimize a single known case:

  tools/regexp/correctness_fuzzer/correctness_fuzzer.py --ref A --test B \
      --pattern '(?=|)()x|' --flags '' --subject z

The reference is treated as ground truth; only cases the reference
executes cleanly are compared, so an unsupported-syntax difference in
the reference is never reported as a test failure.
"""

import argparse
import collections
import contextlib
import json
import os
import random
import re
import subprocess
import sys
import tempfile

import grammar

# The d8 harness that actually constructs and runs each regexp, kept next to
# this script so it ships with the tool instead of being written at runtime.
# realpath so the harness is found even when the script is invoked via symlink.
HARNESS_PATH = os.path.join(
    os.path.dirname(os.path.realpath(__file__)), "harness.js")


def parse_config(spec):
  """'path:--flag --flag' -> (path, [flags]).

  Splits on the first colon.  A leading flag-less path (no colon) yields
  an empty flag list.
  """
  parts = spec.split(":", 1)
  path = parts[0]
  flags = parts[1].split() if len(parts) > 1 and parts[1] else []
  return path, flags


# ---- Execution + diff -----------------------------------------------------

# Sentinel return code for a timed-out run, distinct from a real process exit
# status.  A timeout establishes no ground truth; a genuine nonzero exit is a
# hard crash (CHECK/DCHECK failure, segfault) and a finding in its own right.
TIMEOUT_RC = -99


class Runner:

  def __init__(self, ref_spec, test_spec):
    self.ref = parse_config(ref_spec)
    self.test = parse_config(test_spec)

  @contextlib.contextmanager
  def _cases_file(self, cases):
    fd, path = tempfile.mkstemp(prefix="regexp_fuzz_", suffix=".json")
    try:
      with os.fdopen(fd, "w") as f:
        json.dump(cases, f)
      yield path
    finally:
      os.unlink(path)

  def _run(self, config, casefile):
    path, flags = config
    try:
      p = subprocess.run([path, *flags, HARNESS_PATH, "--", casefile],
                         capture_output=True,
                         text=True,
                         timeout=60)
      return p.returncode, p.stdout
    except subprocess.TimeoutExpired:
      return TIMEOUT_RC, ""

  @staticmethod
  def _results(out):
    """Map case index -> result JSON for lines shaped '<int>\\t<json>'.

    Selecting by shape keeps the diff robust to any banner or warning
    lines a d8 configuration might print around the harness output.
    """
    results = {}
    for line in out.splitlines():
      key, tab, value = line.partition("\t")
      if tab and key.isdigit():
        results[int(key)] = value
    return results

  def run_one(self, config, pattern, flags, subject, last_index=0):
    with self._cases_file([[pattern, flags, subject, last_index]]) as casefile:
      rc, out = self._run(config, casefile)
    return rc, self._results(out).get(0)

  def finding(self, pattern, flags, subject, last_index=0):
    """Classify a case as a finding, or return None.

    'CRASH' when either configuration aborts -- a CHECK/DCHECK failure or
    segfault, distinct from a syntax error, which the harness reports as a
    clean 'ERR_' result.  'COLD/WARM' when one configuration disagrees with
    itself across the two execs, which is a bug in that configuration alone.
    'DIVERGENCE' when both run cleanly but the test disagrees with the
    reference.  A reference syntax error, or a timeout on either side,
    establishes no ground truth and is skipped.
    """
    rc_r, res_r = self.run_one(self.ref, pattern, flags, subject, last_index)
    rc_t, res_t = self.run_one(self.test, pattern, flags, subject, last_index)
    return classify(rc_r, res_r, rc_t, res_t)

  def run_batch(self, cases):
    with self._cases_file(cases) as casefile:
      rc_r, out_r = self._run(self.ref, casefile)
      rc_t, out_t = self._run(self.test, casefile)
    ref, test = self._results(out_r), self._results(out_t)
    for i in range(len(cases)):
      # Classified rather than string-compared, so the batch scan and finding()
      # agree on what counts.  A raw diff would flag every case the reference
      # merely rejects, which finding() then declines to reproduce -- leaving
      # an unminimizable "finding" that the ground-truth policy says to skip.
      #
      # Both return codes are passed as 0: they belong to the whole process,
      # not to this case, and feeding a crashed run's code in here would
      # classify every case from the first one onward as a CRASH and report
      # index 0 rather than the case that actually aborted.  That is what the
      # rc check below is for.
      if classify(0, ref.get(i), 0, test.get(i)):
        return i
    # Identical output but a hard exit on either side is a crash that
    # truncated both runs at the same case (e.g. a shared DCHECK), which the
    # diff above cannot see.  The first un-emitted case is the repro.
    if rc_r not in (0, TIMEOUT_RC) or rc_t not in (0, TIMEOUT_RC):
      return min(len(ref), len(cases) - 1)
    return None


def _no_ground_truth(res):
  """Does |res| leave the reference without an answer to compare against?

  A missing line, or any error marker: a pattern the reference rejects
  outright (ERR_CTOR) or one whose exec raised (ERR_EXEC), which a stack or
  backtrack limit makes configuration-dependent rather than a wrong answer.
  """
  if res is None:
    return True
  try:
    parsed = json.loads(res)
  except json.JSONDecodeError:
    return True
  if isinstance(parsed, str):
    return parsed.startswith("ERR_")
  return any(
      isinstance(v, str) and v.startswith("ERR_") for v in parsed.values())


def _self_inconsistent(res):
  """Do the two execs of one configuration disagree with each other?

  Both run a freshly constructed JSRegExp over the same subject from the same
  lastIndex, so they must agree whatever the answer is; only what the engine
  has cached by the second one differs.  This holds within a single
  configuration, so it catches a bug the reference and the test share.
  """
  if res is None:
    return False
  try:
    parsed = json.loads(res)
  except json.JSONDecodeError:
    return False
  if not isinstance(parsed, dict):
    return False
  return parsed.get("cold") != parsed.get("warm")


def classify(rc_ref, res_ref, rc_test, res_test):
  """Classify one case's pair of results, or return None if it is clean."""
  if rc_ref == TIMEOUT_RC or rc_test == TIMEOUT_RC:
    return None
  if rc_ref != 0 or rc_test != 0:
    return "CRASH"  # a hard abort on either side is a bug regardless
  # Checked before the ground-truth filter: a configuration contradicting
  # itself needs no external reference to be wrong.
  if _self_inconsistent(res_ref) or _self_inconsistent(res_test):
    return "COLD/WARM"
  if _no_ground_truth(res_ref):
    return None
  return "DIVERGENCE" if res_test != res_ref else None


# ---- Minimization ---------------------------------------------------------


def _shrink_targets(n):
  """Candidate smaller values for a quantifier bound, best first.

  0 and 1 first because they are the shapes that usually still reproduce and
  read best in a repro, then a halving sequence so a large bound converges in
  log steps instead of one re-run of both configurations per unit.
  """
  seen = set()
  for c in (0, 1, n // 2, n * 3 // 4, n - 1):
    if 0 <= c < n and c not in seen:
      seen.add(c)
      yield c


def _quantifier(lo, bounded, hi):
  """Rebuild `{n}` (bounded=False) or `{n,}` / `{n,m}` (bounded=True)."""
  if not bounded:
    return "{%d}" % lo
  return "{%d,%s}" % (lo, "" if hi is None else "%d" % hi)


def minimize(runner, pattern, flags, subject, last_index=0):

  def cands(p):
    n = len(p)
    for size in (32, 16, 8, 4, 2, 1):
      for s in range(n - size + 1):
        yield p[:s] + p[s + size:]
    # Quantifier bounds are shrunk toward zero by halving rather than by
    # decrementing: a bound in the hundreds would otherwise need one full
    # re-run of both configurations per step.
    for m in re.finditer(r"\{(\d+)(,)?(\d+)?\}", p):
      lo = int(m.group(1))
      bounded = m.group(2) is not None
      hi = int(m.group(3)) if m.group(3) is not None else None
      for lo_try in _shrink_targets(lo):
        if hi is not None and lo_try > hi:
          continue
        yield p[:m.start()] + _quantifier(lo_try, bounded, hi) + p[m.end():]
      if hi is not None:
        for hi_try in _shrink_targets(hi):
          if hi_try >= lo:
            yield p[:m.start()] + _quantifier(lo, bounded, hi_try) + p[m.end():]

  def size(p):
    # Length alone would reject every same-length quantifier shrink ({5} to
    # {0}), which is most of them; the bound total breaks the tie.
    return (len(p),
            sum(
                int(m.group(1)) + int(m.group(3) or 0)
                for m in re.finditer(r"\{(\d+)(,)?(\d+)?\}", p)))

  improved = True
  while improved:
    improved = False
    for c in cands(pattern):
      if size(c) >= size(pattern):
        continue
      try:
        if runner.finding(c, flags, subject, last_index):
          pattern = c
          improved = True
          break
      except Exception:
        pass
  # Shrink the subject.
  changed = True
  while changed:
    changed = False
    for chunk in (4, 2, 1):
      for s in range(len(subject) - chunk + 1):
        c = subject[:s] + subject[s + chunk:]
        if runner.finding(pattern, flags, c, last_index):
          subject = c
          changed = True
          break
      if changed:
        break
  # Drop flags that aren't load-bearing.
  for f in list(flags):
    cand = flags.replace(f, "")
    if runner.finding(pattern, cand, subject, last_index):
      flags = cand
  # A zero lastIndex is the simpler repro; keep the nonzero one only when the
  # finding needs it.
  if last_index and runner.finding(pattern, flags, subject, 0):
    last_index = 0
  return pattern, flags, subject, last_index


def _print_case(runner, pattern, flags, subject, last_index, header):
  rc_r, res_r = runner.run_one(runner.ref, pattern, flags, subject, last_index)
  rc_t, res_t = runner.run_one(runner.test, pattern, flags, subject, last_index)
  print(header)
  print("  pattern: /%s/%s" % (pattern, flags))
  print("  subject: %r" % subject)
  if last_index:
    print("  lastIndex: %d" % last_index)
  print("  ref : rc=%d %s" % (rc_r, res_r))
  print("  test: rc=%d %s" % (rc_t, res_t))
  sys.stdout.flush()


def report(runner, pattern, flags, subject, last_index, header):
  # Minimization can take a while on a large pattern, and a Ctrl-C during it
  # (the usual way a long run is stopped) would otherwise discard the finding
  # before anything is printed.  On interrupt emit the un-minimized case, which
  # is still a valid repro, before propagating.
  try:
    pattern, flags, subject, last_index = minimize(runner, pattern, flags,
                                                   subject, last_index)
  except KeyboardInterrupt:
    _print_case(runner, pattern, flags, subject, last_index,
                header + " (unminimized)")
    raise
  _print_case(runner, pattern, flags, subject, last_index, header)


def _print_coverage(coverage):
  """Report per-rule expansion counts, unexercised rules last.

  A rule that never fired is the actionable part: it means the run tested
  nothing about that grammar alternative, which no amount of case volume
  reveals on its own.
  """
  rules = grammar.all_rules()
  used = [(p, n) for (p, n) in rules if coverage[(p, n)]]
  missed = [(p, n) for (p, n) in rules if not coverage[(p, n)]]
  total = sum(coverage.values()) or 1
  print("\ngrammar coverage: %d/%d rules, %d expansions" %
        (len(used), len(rules), total))
  for prod, name in sorted(used, key=lambda k: -coverage[k]):
    count = coverage[(prod, name)]
    print("  %-46s %8d  %5.2f%%" % ("%s.%s" %
                                    (prod, name), count, 100.0 * count / total))
  if missed:
    print("  NOT EXERCISED:")
    for prod, name in missed:
      print("    %s.%s" % (prod, name))


def main():
  ap = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  ap.add_argument(
      "--ref",
      required=True,
      help="reference d8 (ground truth), 'path' or 'path:--flags'")
  ap.add_argument(
      "--test", required=True, help="d8 under test, 'path' or 'path:--flags'")
  ap.add_argument(
      "--seed",
      type=int,
      default=None,
      help="base seed; randomized and printed when omitted")
  ap.add_argument("--batches", type=int, default=100)
  ap.add_argument("--batch-size", type=int, default=500)
  ap.add_argument("--max-findings", type=int, default=20)
  ap.add_argument(
      "--progress-every",
      type=int,
      default=25,
      help="print a flushed progress line every N clean batches (0 disables); "
      "the running trail is what survives an early Ctrl-C")
  ap.add_argument(
      "--profile",
      default="default",
      choices=sorted(grammar.PROFILES),
      help="weight overlay aiming generation at a family of patterns")
  ap.add_argument(
      "--weight",
      action="append",
      metavar="Production.rule=N",
      help="multiply one grammar rule's weight; repeatable")
  ap.add_argument(
      "--max-depth",
      type=int,
      default=5,
      help="derivation depth budget; larger means bigger nested patterns. "
      "The default is the smallest that reaches every grammar rule -- the "
      "deepest v-mode class nesting needs 5 (verify with --coverage)")
  ap.add_argument(
      "--coverage",
      action="store_true",
      help="report which grammar rules the run exercised, and which it missed")
  ap.add_argument(
      "--pattern", help="reproduce a single case instead of fuzzing")
  ap.add_argument("--flags", default="")
  ap.add_argument("--subject", default="")
  ap.add_argument("--last-index", type=int, default=0)
  args = ap.parse_args()

  try:
    weights = grammar.parse_weights(args.profile, args.weight)
  except ValueError as e:
    ap.error(str(e))

  runner = Runner(args.ref, args.test)

  if args.pattern is not None:
    kind = runner.finding(args.pattern, args.flags, args.subject,
                          args.last_index)
    if kind:
      report(runner, args.pattern, args.flags, args.subject, args.last_index,
             kind)
      return 1
    print("no divergence for the given case")
    return 0

  seed = args.seed if args.seed is not None else random.randrange(2**32)
  # Print (flushed) up front so the seed survives an early abort; a long run is
  # typically stopped with Ctrl-C, and the seed is what makes it reproducible.
  print("seed: %d (pass --seed %d to reproduce)" % (seed, seed))
  sys.stdout.flush()

  findings = 0
  done = 0
  coverage = collections.Counter() if args.coverage else None
  # A long run is usually ended by Ctrl-C or an external kill.  Catch the
  # interrupt so the summary of what completed still prints; report() has
  # already flushed each finding as it was found.
  try:
    for it in range(args.batches):
      r = random.Random(seed * 1_000_003 + it)
      cases = [
          grammar.gen_case(r, args.max_depth, weights, coverage)
          for _ in range(args.batch_size)
      ]
      idx = runner.run_batch(cases)
      done = it + 1
      if idx is None:
        # Emit a flushed heartbeat so a clean run leaves a trail (cases fuzzed,
        # findings so far) that survives an abort instead of only the final
        # summary.
        if args.progress_every and done % args.progress_every == 0:
          print("progress: %d/%d batches, %d finding(s)" %
                (done, args.batches, findings))
          sys.stdout.flush()
        continue
      pat, fl, sub, li = cases[idx]
      kind = runner.finding(pat, fl, sub, li) or "DIVERGENCE"
      report(runner, pat, fl, sub, li, "%s batch=%d case=%d" % (kind, it, idx))
      findings += 1
      if findings >= args.max_findings:
        print("reached --max-findings; stopping")
        break
  except KeyboardInterrupt:
    print("\ninterrupted after %d batch(es)" % done)
  print("done: %d/%d batches x %d cases, %d divergence(s)" %
        (done, args.batches, args.batch_size, findings))
  if coverage is not None:
    _print_coverage(coverage)
  sys.stdout.flush()
  return 1 if findings else 0


if __name__ == "__main__":
  sys.exit(main())
