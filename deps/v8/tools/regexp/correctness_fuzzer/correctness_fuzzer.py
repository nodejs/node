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
import contextlib
import json
import os
import random
import re
import subprocess
import sys
import tempfile

# The d8 harness that actually constructs and runs each regexp, kept next to
# this script so it ships with the tool instead of being written at runtime.
# realpath so the harness is found even when the script is invoked via symlink.
HARNESS_PATH = os.path.join(
    os.path.dirname(os.path.realpath(__file__)), "harness.js")

# Character menu kept small but spanning the interesting axes: ASCII
# letters/digits/punctuation, a Latin-1 supplement char, a BMP two-byte
# char, and a supplementary (surrogate-pair) char.
CHARS = list("abcxyz019:.-") + ["é", "Ω", "\U0001f0a1"]


def parse_config(spec):
  """'path:--flag --flag' -> (path, [flags]).

  Splits on the first colon.  A leading flag-less path (no colon) yields
  an empty flag list.
  """
  parts = spec.split(":", 1)
  path = parts[0]
  flags = parts[1].split() if len(parts) > 1 and parts[1] else []
  return path, flags


# ---- Random regexp generation --------------------------------------------

# Supplementary (surrogate-pair) code points from the playing-card block:
# these exercise the two-code-unit text and class-lowering paths.
_SUPP = [chr(0x1F0A1 + i) for i in range(14)
        ] + [chr(0x1F0B1 + i) for i in range(14)]

# Range endpoints in ascending code-point order, so a range built from two
# indices i <= j is always well-formed and not rejected as out-of-order.
_CLASS_ATOMS = "0123456789abcdefxyz"


def rand_class(r):
  if r.random() < 0.18:
    # Supplementary class: a few code points and/or a range, e.g.
    # [\u{1f0a1}-\u{1f0ae}].
    parts = []
    for _ in range(r.randint(1, 3)):
      if r.random() < 0.5:
        a = r.choice(_SUPP)
        b = chr(ord(a) + r.randint(0, 6))
        parts.append("%s-%s" % (a, b))
      else:
        parts.append(r.choice(_SUPP))
    neg = "^" if r.random() < 0.2 else ""
    return "[%s%s]" % (neg, "".join(parts))
  parts = []
  for _ in range(r.randint(1, 4)):
    if r.random() < 0.4:
      i = r.randrange(len(_CLASS_ATOMS))
      j = r.randint(i, min(i + 6, len(_CLASS_ATOMS) - 1))
      parts.append("%s-%s" % (_CLASS_ATOMS[i], _CLASS_ATOMS[j]))
    else:
      parts.append(r.choice("abcxyz019"))
  neg = "^" if r.random() < 0.2 else ""
  return "[%s%s]" % (neg, "".join(parts))


def rand_atom(r, depth):
  roll = r.random()
  if roll < 0.35:
    return r.choice("abcxyz019:.") if r.random() < 0.8 else r.choice(CHARS)
  if roll < 0.5:
    return rand_class(r)
  if roll < 0.58:
    return r.choice([".", r"\d", r"\w", r"\s", r"\b", r"\B", "^", "$"])
  if roll < 0.64 and depth < 3:
    body = rand_re(r, depth + 1)
    return "(" + body + ")" + (r"\1" if r.random() < 0.15 else "")
  if roll < 0.74 and depth < 3:
    kind = r.choice(["(?:", "(?=", "(?!", "(?<=", "(?<!", "("])
    return kind + rand_re(r, depth + 1) + ")"
  return r.choice("abcxyz")


def rand_piece(r, depth):
  a = rand_atom(r, depth)
  if r.random() < 0.25:
    q = r.choice(["*", "+", "?", "{0,2}", "{1,3}", "{2}", "{2,}", "{1,4}"])
    if r.random() < 0.3:
      q += "?"
    return a + q
  return a


def rand_seq(r, depth):
  return "".join(rand_piece(r, depth) for _ in range(r.randint(1, 5)))


def rand_re(r, depth=0):
  n = r.randint(1, 4)
  alts = [rand_seq(r, depth) for _ in range(n)]
  if r.random() < 0.15:
    alts[r.randrange(n)] = ""  # empty alternative
  return "|".join(alts)


def rand_subject(r):
  # Bias toward supplementary code points sometimes so surrogate-pair
  # texts get matching input.
  pool = CHARS + (_SUPP if r.random() < 0.5 else [])
  return "".join(r.choice(pool) for _ in range(r.randint(0, 14)))


def gen_case(r):
  pat = rand_re(r)
  flags = "".join(r.sample(["i", "m", "s", "y"], r.randint(0, 2)))
  # 'u' and 'v' are mutually exclusive unicode modes; pick at most one,
  # biased to include one since they change surrogate handling and class
  # lowering materially.  Favor 'v' (unicodeSets): it is the newer mode with
  # its own, more intricate class-lowering path.
  roll = r.random()
  if roll < 0.4:
    flags += "v"
  elif roll < 0.55:
    flags += "u"
  return [pat, flags, rand_subject(r)]


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

  def run_one(self, config, pattern, flags, subject):
    with self._cases_file([[pattern, flags, subject]]) as casefile:
      rc, out = self._run(config, casefile)
    return rc, self._results(out).get(0)

  def finding(self, pattern, flags, subject):
    """Classify a case as a finding, or return None.

    'CRASH' when either configuration aborts -- a CHECK/DCHECK failure or
    segfault, distinct from a syntax error, which the harness reports as a
    clean 'ERR_' result.  'DIVERGENCE' when both run cleanly but the test
    disagrees with the reference.  A reference syntax error, or a timeout on
    either side, establishes no ground truth and is skipped.
    """
    rc_r, res_r = self.run_one(self.ref, pattern, flags, subject)
    rc_t, res_t = self.run_one(self.test, pattern, flags, subject)
    if rc_r == TIMEOUT_RC or rc_t == TIMEOUT_RC:
      return None
    if rc_r != 0 or rc_t != 0:
      return "CRASH"  # a hard abort on either side is a bug regardless
    if res_r is None or res_r.startswith('"ERR_'):
      return None  # reference doesn't establish ground truth here
    return "DIVERGENCE" if res_t != res_r else None

  def run_batch(self, cases):
    with self._cases_file(cases) as casefile:
      rc_r, out_r = self._run(self.ref, casefile)
      rc_t, out_t = self._run(self.test, casefile)
    ref, test = self._results(out_r), self._results(out_t)
    # First case whose result differs, including one side missing it (a
    # crash or timeout that truncated that configuration's output).
    for i in range(len(cases)):
      if ref.get(i) != test.get(i):
        return i
    # Identical output but a hard exit on either side is a crash that
    # truncated both runs at the same case (e.g. a shared DCHECK), which the
    # diff above cannot see.  The first un-emitted case is the repro.
    if rc_r not in (0, TIMEOUT_RC) or rc_t not in (0, TIMEOUT_RC):
      return min(len(ref), len(cases) - 1)
    return None


# ---- Minimization ---------------------------------------------------------


def minimize(runner, pattern, flags, subject):

  def cands(p):
    n = len(p)
    for size in (32, 16, 8, 4, 2, 1):
      for s in range(n - size + 1):
        yield p[:s] + p[s + size:]
    for m in re.finditer(r"\{(\d+)(,(\d+)?)?\}", p):
      a = int(m.group(1))
      if a > 0:
        yield p[:m.start()] + m.group(0).replace(str(a), str(a - 1),
                                                 1) + p[m.end():]
    yield p.replace("[0-9a-f]", "[0-9]", 1)

  improved = True
  while improved:
    improved = False
    for c in cands(pattern):
      if len(c) >= len(pattern):
        continue
      try:
        if runner.finding(c, flags, subject):
          pattern = c
          improved = True
          break
      except Exception:
        pass
  # Shrink the subject.
  changed = True
  while changed:
    changed = False
    for size in (4, 2, 1):
      for s in range(len(subject) - size + 1):
        c = subject[:s] + subject[s + size:]
        if runner.finding(pattern, flags, c):
          subject = c
          changed = True
          break
      if changed:
        break
  # Drop flags that aren't load-bearing.
  for f in list(flags):
    cand = flags.replace(f, "")
    if runner.finding(pattern, cand, subject):
      flags = cand
  return pattern, flags, subject


def report(runner, pattern, flags, subject, header):
  pattern, flags, subject = minimize(runner, pattern, flags, subject)
  rc_r, res_r = runner.run_one(runner.ref, pattern, flags, subject)
  rc_t, res_t = runner.run_one(runner.test, pattern, flags, subject)
  print(header)
  print("  pattern: /%s/%s" % (pattern, flags))
  print("  subject: %r" % subject)
  print("  ref : rc=%d %s" % (rc_r, res_r))
  print("  test: rc=%d %s" % (rc_t, res_t))
  sys.stdout.flush()


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
      "--pattern", help="reproduce a single case instead of fuzzing")
  ap.add_argument("--flags", default="")
  ap.add_argument("--subject", default="")
  args = ap.parse_args()

  runner = Runner(args.ref, args.test)

  if args.pattern is not None:
    kind = runner.finding(args.pattern, args.flags, args.subject)
    if kind:
      report(runner, args.pattern, args.flags, args.subject, kind)
      return 1
    print("no divergence for the given case")
    return 0

  seed = args.seed if args.seed is not None else random.randrange(2**32)
  if args.seed is None:
    print("seed: %d (pass --seed %d to reproduce)" % (seed, seed))

  findings = 0
  for it in range(args.batches):
    r = random.Random(seed * 1_000_003 + it)
    cases = [gen_case(r) for _ in range(args.batch_size)]
    idx = runner.run_batch(cases)
    if idx is None:
      continue
    pat, fl, sub = cases[idx]
    kind = runner.finding(pat, fl, sub) or "DIVERGENCE"
    report(runner, pat, fl, sub, "%s batch=%d case=%d" % (kind, it, idx))
    findings += 1
    if findings >= args.max_findings:
      print("reached --max-findings; stopping")
      break
  print("done: %d batches x %d cases, %d divergence(s)" %
        (args.batches, args.batch_size, findings))
  return 1 if findings else 0


if __name__ == "__main__":
  sys.exit(main())
