# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Flags, subjects, and whole cases."""

from .menus import (LAST_INDEX_SHARE, SUBJECT_LITERAL_SHARE, SUBJECT_MAX_LEN,
                    SUBJECT_NOISE)
from .registry import Context
from .rules import gen_pattern

# Flags outside the mode pair, with the weight each is picked at.  Not
# uniform: `i` rewrites every character and class in the pattern and `g` and
# `d` change what the result even contains, while `m` and `s` each retarget a
# single construct and `y` only removes the search loop.
_FLAG_WEIGHTS = {
    "i": 0.55,
    "g": 0.35,
    "d": 0.25,
    "m": 0.20,
    "s": 0.20,
    "y": 0.15,
}

# `u` and `v` are mutually exclusive, so they are drawn separately.
_UNICODE_MODE_SHARE = 0.20
_UNICODE_SETS_MODE_SHARE = 0.30


def gen_flags(rng):
  """Pick a flag set; `u` and `v` are mutually exclusive."""
  flags = "".join(f for f, w in _FLAG_WEIGHTS.items() if rng.random() < w)
  roll = rng.random()
  if roll < _UNICODE_SETS_MODE_SHARE:
    flags += "v"
  elif roll < _UNICODE_SETS_MODE_SHARE + _UNICODE_MODE_SHARE:
    flags += "u"
  return flags


def gen_subject(rng, literals):
  """Build a subject biased toward characters the pattern mentions.

  Drawing from the pattern's own literal pool is what makes a meaningful
  share of cases match; a uniformly random subject almost never gets past
  the first character, leaving the match path untested.
  """
  pool = [c for c in literals if c] or ["a"]
  out = []
  for _ in range(rng.randint(0, SUBJECT_MAX_LEN)):
    take_literal = rng.random() < SUBJECT_LITERAL_SHARE
    out.append(rng.choice(pool if take_literal else SUBJECT_NOISE))
  return "".join(out)


def gen_case(rng, max_depth=5, weights=None, coverage=None):
  """Generate one [pattern, flags, subject, lastIndex] case."""
  flags = gen_flags(rng)
  ctx = Context(
      rng,
      unicode_mode="u" in flags or "v" in flags,
      unicode_sets_mode="v" in flags,
      max_depth=max_depth,
      weights=weights,
      coverage=coverage)
  pattern = gen_pattern(ctx)
  subject = gen_subject(rng, ctx.literals)
  # lastIndex only has an observable effect for sticky/global patterns, but
  # setting it regardless costs nothing and keeps the case shape uniform.
  last_index = 0
  if rng.random() < LAST_INDEX_SHARE:
    last_index = rng.randint(0, max(0, len(subject)))
  return [pattern, flags, subject, last_index]
