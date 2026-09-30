# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Weight overlays that aim a campaign at a family of patterns."""

from .registry import GRAMMAR

# A profile is a table of multipliers keyed "Production.rule", applied on top
# of the base weights.  Aiming a campaign at a feature is a weight change, not
# a code change.
PROFILES = {
    "default": {},
    # Patterns that must match at a fixed position: what a first-character or
    # prefix filter optimization actually sees.
    "anchored": {
        "Assertion.caret": 8.0,
        "Term.assertion": 2.0,
        "Disjunction.alternation": 2.0,
        "Atom.character_class": 2.0,
    },
    # v-mode set algebra: nested classes, intersection, subtraction, strings.
    "classes": {
        "Atom.character_class": 4.0,
        "ClassSetExpression.class_intersection": 3.0,
        "ClassSetExpression.class_subtraction": 3.0,
        "ClassSetOperand.nested_class": 3.0,
        "ClassSetOperand.nested_negated_class": 3.0,
        "ClassSetOperand.class_string_disjunction": 3.0,
        "CharacterClassEscape.unicode_property_of_strings": 3.0,
        "Atom.pattern_character": 0.3,
    },
    # Shapes that stress backtracking: nested quantifiers and alternations.
    "backtracking": {
        "Term.quantified_atom": 4.0,
        "Disjunction.alternation": 3.0,
        "Atom.non_capturing_group": 3.0,
        "Atom.capturing_group": 2.0,
        "QuantifierPrefix.star": 2.0,
        "QuantifierPrefix.plus": 2.0,
    },
    # Counted quantifiers above the compiler's unroll threshold, which build a
    # counter-based loop instead of a repeated body.
    "loops": {
        "Term.quantified_atom": 4.0,
        "QuantifierPrefix.exactly_large": 6.0,
        "QuantifierPrefix.at_least_large": 6.0,
        "QuantifierPrefix.bounded_large": 6.0,
        "QuantifierPrefix.star": 0.3,
        "QuantifierPrefix.plus": 0.3,
        "QuantifierPrefix.optional": 0.3,
    },
    # Lookarounds and references, which constrain without consuming.
    "lookaround": {
        "Assertion.lookahead": 5.0,
        "Assertion.negative_lookahead": 5.0,
        "Assertion.lookbehind": 5.0,
        "Assertion.negative_lookbehind": 5.0,
        "AtomEscape.named_backreference": 3.0,
        "AtomEscape.decimal_escape": 3.0,
        "Atom.named_group": 3.0,
    },
}


def parse_weights(profile, overrides):
  """Combine a named profile with explicit 'Production.rule=N' overrides."""
  if profile not in PROFILES:
    raise ValueError("unknown profile %r (have: %s)" %
                     (profile, ", ".join(sorted(PROFILES))))
  weights = dict(PROFILES[profile])
  for item in overrides or []:
    key, _, value = item.partition("=")
    if not _:
      raise ValueError("expected Production.rule=N, got %r" % item)
    weights[key] = float(value)
  known = {"%s.%s" % (r.prod, r.name) for rs in GRAMMAR.values() for r in rs}
  for key in weights:
    if key not in known:
      raise ValueError("unknown rule %r" % key)
  return weights
