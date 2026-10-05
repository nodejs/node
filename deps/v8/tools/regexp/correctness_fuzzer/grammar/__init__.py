# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Weighted grammar for generating ECMA-262 regexp patterns.

The productions transcribe the pattern grammar of ECMA-262 22.2.1
(Disjunction / Alternative / Term / Atom / ClassContents / ...), one
`@rule` per grammar alternative, so a reader can check the generator
against the spec line by line.  Generation is a weighted random
derivation: each rule carries a weight, and a rule whose `guard` is
false for the current parameters is not eligible at all.

Two properties follow from mirroring the spec rather than emitting
strings ad hoc:

  * The spec's own parameters -- [UnicodeMode], [UnicodeSetsMode],
    [NamedCaptureGroups] -- are threaded through the derivation and
    gate the rules the spec gates, so mode-specific syntax (`\\q{}`,
    `&&`, `\\p{}`) is only ever emitted in a mode that accepts it.
    Most syntax errors are structurally impossible instead of merely
    unlikely.
  * Coverage is measurable: every expansion records the rule it took,
    so a run can report which grammar alternatives it never exercised.

Derivation is bounded by a depth budget.  Rules marked `recursive`
re-enter the grammar; once the budget is spent they become ineligible
and only terminal rules remain, which terminates the derivation and
keeps the size distribution controllable via --max-depth.

Weights are deliberately data, not control flow: a profile is a table
of multipliers applied to named rules (see PROFILES), so a campaign can
be aimed at a feature -- anchored patterns, v-mode class set algebra,
backtracking shapes -- without touching generation logic.  A rule makes
no weighted choice of its own for the same reason: a branch inside a
rule body would be a weight that neither --weight nor --coverage can
see.

Modules:
  registry  -- the @rule decorator, Context, and expand().
  menus     -- character pools and generation constants.
  rules     -- the grammar itself, one @rule per spec alternative.
  cases     -- flags, subjects, and whole [pattern, flags, subject, li] cases.
  profiles  -- weight overlays (--profile) and --weight parsing.
"""

from .cases import gen_case, gen_flags, gen_subject
from .profiles import PROFILES, parse_weights
from .registry import Context, GrammarError, all_rules, expand

__all__ = [
    "Context",
    "GrammarError",
    "PROFILES",
    "all_rules",
    "expand",
    "gen_case",
    "gen_flags",
    "gen_subject",
    "parse_weights",
]
