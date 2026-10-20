# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""The ECMA-262 22.2.1 pattern grammar, one @rule per alternative.

The productions transcribe the spec's pattern grammar (Disjunction /
Alternative / Term / Atom / ClassContents / ...) so a reader can check the
generator against the spec line by line.  Where the spec gates an
alternative on a parameter -- [UnicodeMode], [UnicodeSetsMode] -- the rule
carries the matching guard, so mode-specific syntax is only ever emitted in
a mode that accepts it.

Every choice a rule makes between meaningfully different outputs is a
separate rule with its own weight, not a branch inside one.  A branch would
be a second weight system: invisible to the coverage report and unreachable
from --weight, so a campaign could not aim at it and a run could not tell
you it never fired.
"""

from .menus import (ASCII_LETTER, CLASS_SET_SAFE, CONTROL_ESCAPE,
                    ESCAPE_LITERALS, LATIN1_ONLY, LITERAL, LITERAL_WIDE,
                    RANGE_ALPHABET, UNICODE_PROPERTIES,
                    UNICODE_PROPERTIES_OF_STRINGS)
from .registry import (NOT_SETS, NOT_UNICODE, SETS, UNICODE, concat_terms,
                       expand, repeat, rule)


def _random_range(ctx):
  """A `a-z` class range, drawn from an ordered alphabet so it is well formed."""
  i = ctx.rng.randrange(len(RANGE_ALPHABET))
  j = min(i + ctx.rng.randint(0, 6), len(RANGE_ALPHABET) - 1)
  ctx.note_literal(RANGE_ALPHABET[i])
  return "%s-%s" % (RANGE_ALPHABET[i], RANGE_ALPHABET[j])


# ---- Pattern / Disjunction / Alternative / Term ---------------------------
# Pattern :: Disjunction


def gen_pattern(ctx):
  return expand(ctx, "Disjunction")


# Disjunction :: Alternative
#                Alternative `|` Disjunction


@rule("Disjunction", weight=6.0)
def single(ctx):
  return expand(ctx, "Alternative")


@rule("Disjunction", weight=3.0, recursive=True)
def alternation(ctx):
  return expand(ctx, "Alternative") + "|" + expand(ctx, "Disjunction")


# Alternative :: [empty]
#                Alternative Term


@rule("Alternative", weight=0.6)
def empty(ctx):
  return ""


@rule("Alternative", weight=9.0, recursive=True)
def terms(ctx):
  return repeat(ctx, "Term", 1, 4)


# Term :: Assertion
#         Atom
#         Atom Quantifier


@rule("Term", weight=1.5)
def assertion(ctx):
  return expand(ctx, "Assertion")


@rule("Term", weight=6.0, recursive=True)
def atom(ctx):
  return expand(ctx, "Atom")


@rule("Term", weight=3.0, recursive=True)
def quantified_atom(ctx):
  return expand(ctx, "Atom") + expand(ctx, "Quantifier")


# Assertion :: `^` `$` `\b` `\B`
#              `(?=` Disjunction `)` and the three other lookaround forms


@rule("Assertion", weight=2.0)
def caret(ctx):
  return "^"


@rule("Assertion", weight=1.5)
def dollar(ctx):
  return "$"


@rule("Assertion", weight=1.0)
def word_boundary(ctx):
  return r"\b"


@rule("Assertion", weight=0.7)
def not_word_boundary(ctx):
  return r"\B"


@rule("Assertion", weight=1.0, recursive=True)
def lookahead(ctx):
  return "(?=" + expand(ctx, "Disjunction") + ")"


@rule("Assertion", weight=0.8, recursive=True)
def negative_lookahead(ctx):
  return "(?!" + expand(ctx, "Disjunction") + ")"


@rule("Assertion", weight=1.0, recursive=True)
def lookbehind(ctx):
  return "(?<=" + expand(ctx, "Disjunction") + ")"


@rule("Assertion", weight=0.8, recursive=True)
def negative_lookbehind(ctx):
  return "(?<!" + expand(ctx, "Disjunction") + ")"


# Quantifier :: QuantifierPrefix
#               QuantifierPrefix `?`


@rule("Quantifier", weight=3.0)
def greedy(ctx):
  return expand(ctx, "QuantifierPrefix")


@rule("Quantifier", weight=1.0)
def lazy(ctx):
  return expand(ctx, "QuantifierPrefix") + "?"


# QuantifierPrefix :: `*` `+` `?` `{n}` `{n,}` `{n,m}`
#
# The compiler unrolls a quantifier whose bounds are at most
# kMaxUnrolledMinMatches / kMaxUnrolledMaxMatches (both 3, see
# regexp-compiler-tonode.cc).  Bounds drawn only from that range would leave
# the counter-based loop -- a different node shape with its own register
# handling -- entirely ungenerated, so each counted form has a small and a
# large variant.  The large ones stay well under the point where compilation
# cost is noticeable.


@rule("QuantifierPrefix", weight=3.0)
def star(ctx):
  return "*"


@rule("QuantifierPrefix", weight=3.0)
def plus(ctx):
  return "+"


@rule("QuantifierPrefix", weight=3.0)
def optional(ctx):
  return "?"


@rule("QuantifierPrefix", weight=1.0)
def exactly(ctx):
  return "{%d}" % ctx.rng.randint(0, 3)


@rule("QuantifierPrefix", weight=0.4)
def exactly_large(ctx):
  return "{%d}" % ctx.rng.randint(4, 40)


@rule("QuantifierPrefix", weight=0.7)
def at_least(ctx):
  return "{%d,}" % ctx.rng.randint(0, 2)


@rule("QuantifierPrefix", weight=0.3)
def at_least_large(ctx):
  return "{%d,}" % ctx.rng.randint(4, 40)


@rule("QuantifierPrefix", weight=1.0)
def bounded(ctx):
  lo = ctx.rng.randint(0, 2)
  return "{%d,%d}" % (lo, lo + ctx.rng.randint(0, 3))


@rule("QuantifierPrefix", weight=0.4)
def bounded_large(ctx):
  lo = ctx.rng.randint(0, 20)
  return "{%d,%d}" % (lo, lo + ctx.rng.randint(4, 60))


# Atom :: PatternCharacter
#         `.`
#         `\` AtomEscape
#         CharacterClass
#         `(` GroupSpecifier? Disjunction `)`
#         `(?` RegularExpressionModifiers `:` Disjunction `)`
#         `(?` Modifiers `-` Modifiers `:` Disjunction `)`


@rule("Atom", weight=9.0)
def pattern_character(ctx):
  return ctx.note_literal(ctx.rng.choice(LITERAL))


@rule("Atom", weight=1.2)
def pattern_character_wide(ctx):
  # A non-Latin-1 literal forces the two-byte subject representation and, for
  # the supplementary one, the surrogate-pair path.
  return ctx.note_literal(ctx.rng.choice(LITERAL_WIDE))


@rule("Atom", weight=1.5)
def dot(ctx):
  return "."


@rule("Atom", weight=3.0)
def atom_escape(ctx):
  return "\\" + expand(ctx, "AtomEscape")


@rule("Atom", weight=4.0, recursive=True)
def character_class(ctx):
  return expand(ctx, "CharacterClass")


@rule("Atom", weight=2.0, recursive=True)
def capturing_group(ctx):
  ctx.capture_count += 1
  return "(" + expand(ctx, "Disjunction") + ")"


@rule("Atom", weight=1.0, recursive=True, guard=lambda c: not c.legacy_k_escape)
def named_group(ctx):
  # Guarded on legacy_k_escape: a `\k` already emitted as an identity escape
  # is only legal while the pattern has no named group at all, and adding one
  # here would retroactively turn that `\k` into an early error.
  ctx.capture_count += 1
  name = "g%d" % len(ctx.group_names)
  ctx.group_names.append(name)
  return "(?<%s>%s)" % (name, expand(ctx, "Disjunction"))


@rule("Atom", weight=2.0, recursive=True)
def non_capturing_group(ctx):
  return "(?:" + expand(ctx, "Disjunction") + ")"


# `(?ims-ims:` ... `)`: at least one modifier, and no modifier may appear on
# both sides of the dash.  The three forms are separate rules because they
# reach different parser paths -- adding, removing, and both at once.


def _modifiers(ctx, n):
  return "".join(ctx.rng.sample("ims", n))


@rule("Atom", weight=1.0, recursive=True)
def modifier_group_add(ctx):
  return "(?%s:%s)" % (_modifiers(ctx, ctx.rng.randint(
      1, 3)), expand(ctx, "Disjunction"))


@rule("Atom", weight=0.6, recursive=True)
def modifier_group_remove(ctx):
  return "(?-%s:%s)" % (_modifiers(ctx, ctx.rng.randint(
      1, 3)), expand(ctx, "Disjunction"))


@rule("Atom", weight=0.6, recursive=True)
def modifier_group_add_remove(ctx):
  mods = _modifiers(ctx, ctx.rng.randint(2, 3))
  cut = ctx.rng.randint(1, len(mods) - 1)
  return "(?%s-%s:%s)" % (mods[:cut], mods[cut:], expand(ctx, "Disjunction"))


# AtomEscape :: DecimalEscape
#               CharacterClassEscape
#               CharacterEscape
#               `k` GroupName


@rule("AtomEscape", weight=1.5, guard=lambda c: c.capture_count > 0)
def decimal_escape(ctx):
  return str(ctx.rng.randint(1, ctx.capture_count))


@rule("AtomEscape", weight=4.0)
def character_class_escape(ctx):
  return expand(ctx, "CharacterClassEscape")


@rule("AtomEscape", weight=3.0)
def character_escape(ctx):
  return expand(ctx, "CharacterEscape")


@rule("AtomEscape", weight=1.5, guard=lambda c: bool(c.group_names))
def named_backreference(ctx):
  return "k<%s>" % ctx.rng.choice(ctx.group_names)


@rule(
    "AtomEscape",
    weight=0.15,
    guard=lambda c: not c.unicode_mode and not c.group_names)
def legacy_k_escape(ctx):
  # `\k` with no named group anywhere in the pattern is an Annex B identity
  # escape matching a literal `k`, and only [~UnicodeMode] admits it -- under
  # `u`/`v`, and under any flags once a named group exists, the same source is
  # an early error.  Both guards are required for this to be a live case
  # rather than one the reference rejects: a syntax error establishes no
  # ground truth, so it is diffed away rather than reported.  named_group is
  # guarded off for the rest of the derivation once this fires.
  ctx.legacy_k_escape = True
  ctx.note_literal("k")
  return "k"


# CharacterEscape :: ControlEscape
#                    `c` AsciiLetter
#                    `0`
#                    HexEscapeSequence
#                    RegExpUnicodeEscapeSequence
#                    IdentityEscape


@rule("CharacterEscape", weight=2.0)
def control_escape(ctx):
  esc = ctx.rng.choice(CONTROL_ESCAPE)
  ctx.note_literal(ESCAPE_LITERALS[esc])
  return esc


@rule("CharacterEscape", weight=1.0)
def control_letter(ctx):
  # `\cA` matches the control character A % 32.
  letter = ctx.rng.choice(ASCII_LETTER)
  ctx.note_literal(chr(ord(letter.upper()) % 32))
  return "c" + letter


@rule("CharacterEscape", weight=0.5)
def null_escape(ctx):
  ctx.note_literal("\0")
  # `\0` carries [lookahead not in DecimalDigit], which concat_terms honours
  # for this escape exactly as it does for a DecimalEscape -- but only between
  # Terms.  A class has no zero-width separator, so spell NUL there as `\x00`,
  # which no following digit can extend.
  return "x00" if ctx.in_class else "0"


@rule("CharacterEscape", weight=1.5)
def hex_escape(ctx):
  # `\xHH` is two hex digits, so the pool has to stay within Latin-1.
  c = ctx.rng.choice(LITERAL + LATIN1_ONLY)
  ctx.note_literal(c)
  return "x%02x" % ord(c)


# RegExpUnicodeEscapeSequence has three spellings, and which are available
# depends on the code point as much as on the mode: `\u{...}` needs
# [+UnicodeMode], and a supplementary code point has no single `\uXXXX` form.
# Each rule therefore picks its character from the pool that its own spelling
# can express, rather than picking a character first and branching.


@rule("CharacterEscape", weight=1.5)
def unicode_escape(ctx):
  c = ctx.rng.choice(LITERAL + ["é", "Ω"])
  ctx.note_literal(c)
  return "u%04x" % ord(c)


@rule("CharacterEscape", weight=0.8, guard=UNICODE)
def unicode_escape_braced(ctx):
  c = ctx.rng.choice(LITERAL + LITERAL_WIDE)
  ctx.note_literal(c)
  return "u{%x}" % ord(c)


@rule("CharacterEscape", weight=0.8, guard=NOT_UNICODE)
def unicode_escape_surrogate_pair(ctx):
  # Without [+UnicodeMode] `\u{...}` is unavailable, so a supplementary code
  # point is spelled as the surrogate pair it is made of.  Under `u`/`v` the
  # pair would instead combine into the single code point, which is a
  # different node shape and is covered by unicode_escape_braced.
  c = ctx.rng.choice([x for x in LITERAL_WIDE if ord(x) > 0xFFFF])
  ctx.note_literal(c)
  cp = ord(c)
  hi = 0xD800 + ((cp - 0x10000) >> 10)
  lo = 0xDC00 + ((cp - 0x10000) & 0x3FF)
  return "u%04x\\u%04x" % (hi, lo)


@rule("CharacterEscape", weight=1.0, guard=UNICODE)
def identity_escape(ctx):
  # [+UnicodeMode] restricts IdentityEscape to SyntaxCharacter and `/`.
  return ctx.rng.choice(list("^$\\.*+?()[]{}|/"))


@rule("CharacterEscape", weight=1.0, guard=NOT_UNICODE)
def identity_escape_extended(ctx):
  # Only [~UnicodeMode] admits arbitrary non-ID characters.
  return ctx.rng.choice(list("^$\\.*+?()[]{}|/-@~"))


# CharacterClassEscape :: `d` `D` `s` `S` `w` `W`
#                         [+UnicodeMode] `p{...}` `P{...}`


@rule("CharacterClassEscape", weight=6.0)
def perl_class(ctx):
  return ctx.rng.choice(list("dDsSwW"))


@rule("CharacterClassEscape", weight=2.0, guard=UNICODE)
def unicode_property(ctx):
  return "%s{%s}" % (ctx.rng.choice("pP"), ctx.rng.choice(UNICODE_PROPERTIES))


@rule(
    "CharacterClassEscape",
    weight=1.0,
    guard=lambda c: c.unicode_sets_mode and not c.no_strings)
def unicode_property_of_strings(ctx):
  # A property of strings can match more than one character, so it is
  # v-mode-only and, like `\q{}`, forbidden inside a negated class.  `\P{}` of
  # one is an early error, hence the positive form only.
  return "p{%s}" % ctx.rng.choice(UNICODE_PROPERTIES_OF_STRINGS)


# CharacterClass :: `[` ClassContents `]`
#                   `[^` ClassContents `]`


@rule("CharacterClass", weight=4.0, recursive=True)
def positive_class(ctx):
  with ctx.inside_class():
    return "[" + expand(ctx, "ClassContents") + "]"


@rule("CharacterClass", weight=1.5, recursive=True)
def negated_class(ctx):
  # "Negated character class may contain strings": a `\q{}` whose alternatives
  # are not all single characters makes the class string-valued, which `[^...]`
  # rejects (MayContainStrings in the spec's static semantics).  The flag
  # suppresses the string disjunction for this subtree.
  with ctx.inside_class(no_strings=True):
    return "[^" + expand(ctx, "ClassContents") + "]"


# ClassContents :: [empty]
#                  [~UnicodeSetsMode] NonemptyClassRanges
#                  [+UnicodeSetsMode] ClassSetExpression


@rule("ClassContents", weight=0.4)
def empty_class(ctx):
  return ""


@rule("ClassContents", weight=9.0, guard=NOT_SETS)
def nonempty_class_ranges(ctx):
  return repeat(ctx, "ClassRangeItem", 1, 4)


@rule("ClassContents", weight=9.0, guard=SETS)
def class_set_expression(ctx):
  return expand(ctx, "ClassSetExpression")


# NonemptyClassRanges, flattened to one item per expansion: a ClassAtom or
# a ClassAtom `-` ClassAtom range.


@rule("ClassRangeItem", weight=5.0)
def class_atom(ctx):
  return expand(ctx, "ClassAtom")


@rule("ClassRangeItem", weight=3.0)
def class_range(ctx):
  return _random_range(ctx)


# ClassAtom :: `-` | ClassAtomNoDash
# ClassAtomNoDash :: SourceCharacter but not one of `\` or `]` or `-`
#                    `\` ClassEscape


@rule("ClassAtom", weight=6.0)
def class_literal(ctx):
  return ctx.note_literal(ctx.rng.choice(LITERAL))


@rule("ClassAtom", weight=0.8)
def class_literal_wide(ctx):
  return ctx.note_literal(ctx.rng.choice(LITERAL_WIDE))


@rule("ClassAtom", weight=2.0)
def class_escape(ctx):
  return "\\" + expand(ctx, "ClassEscape")


# ClassEscape :: `b` | [+UnicodeMode] `-` | CharacterClassEscape
#                | CharacterEscape


@rule("ClassEscape", weight=1.0)
def class_backspace(ctx):
  return "b"


@rule("ClassEscape", weight=0.5, guard=UNICODE)
def class_dash(ctx):
  return "-"


@rule("ClassEscape", weight=4.0)
def class_perl(ctx):
  return expand(ctx, "CharacterClassEscape")


@rule("ClassEscape", weight=3.0)
def class_char_escape(ctx):
  return expand(ctx, "CharacterEscape")


# ---- v-mode class set algebra --------------------------------------------
# ClassSetExpression :: ClassUnion | ClassIntersection | ClassSubtraction


@rule("ClassSetExpression", weight=6.0)
def class_union(ctx):
  return repeat(ctx, "ClassSetOperandOrRange", 1, 3)


@rule("ClassSetExpression", weight=2.5, recursive=True)
def class_intersection(ctx):
  n = 2 if ctx.depth >= ctx.max_depth else ctx.rng.randint(2, 3)
  return "&&".join(expand(ctx, "ClassSetOperand") for _ in range(n))


@rule("ClassSetExpression", weight=2.5, recursive=True)
def class_subtraction(ctx):
  n = 2 if ctx.depth >= ctx.max_depth else ctx.rng.randint(2, 3)
  return "--".join(expand(ctx, "ClassSetOperand") for _ in range(n))


@rule("ClassSetOperandOrRange", weight=3.0)
def set_range(ctx):
  return _random_range(ctx)


@rule("ClassSetOperandOrRange", weight=6.0)
def set_operand(ctx):
  return expand(ctx, "ClassSetOperand")


# ClassSetOperand :: NestedClass | ClassStringDisjunction | ClassSetCharacter


@rule("ClassSetOperand", weight=7.0)
def set_character(ctx):
  return ctx.note_literal(ctx.rng.choice(CLASS_SET_SAFE))


@rule("ClassSetOperand", weight=2.0)
def set_class_escape(ctx):
  return "\\" + expand(ctx, "CharacterClassEscape")


@rule("ClassSetOperand", weight=1.5, recursive=True)
def nested_class(ctx):
  with ctx.inside_class():
    return "[" + expand(ctx, "ClassSetExpression") + "]"


@rule("ClassSetOperand", weight=0.5, recursive=True)
def nested_negated_class(ctx):
  # Negation forbids string-valued contents at any nesting depth, so the ban
  # has to hold for this whole subtree (see negated_class).
  with ctx.inside_class(no_strings=True):
    return "[^" + expand(ctx, "ClassSetExpression") + "]"


@rule("ClassSetOperand", weight=1.5, guard=lambda c: not c.no_strings)
def class_string_disjunction(ctx):
  # `\q{a|bc|}`: a disjunction of literal strings, the one construct that
  # lets a class match more than a single character.
  words = []
  for _ in range(ctx.rng.randint(1, 3)):
    w = "".join(
        ctx.rng.choice(CLASS_SET_SAFE) for _ in range(ctx.rng.randint(0, 3)))
    ctx.note_literal(w)
    words.append(w)
  return r"\q{%s}" % "|".join(words)
