# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Character menus and generation constants.

The pools the rules draw from, kept together so the character-width axes
a run covers are visible in one place instead of spread across rule
bodies.
"""

# PatternCharacter :: SourceCharacter but not SyntaxCharacter.  Characters that
# need no escaping anywhere, plus wide ones for the two-byte / surrogate paths.
# The punctuation is load-bearing: letters and digits share their high bits, so
# an alternation over them still admits a discriminating bitmask, while
# punctuation spans the ASCII table and collapses one -- the shape a tokenizer's
# operator alternation has.
LITERAL = list("abcxyzABXY019") + list("=:;,~!@%&<>")
LITERAL_WIDE = ["é", "Ω", "\U0001f0a1"]

# `\xHH` reaches U+00FF and no further, so its pool is the Latin-1 subset:
# LITERAL_WIDE's other members would silently truncate ("x%02x" % 0x3A9 is
# "x3a9", which reads as `\x3a` followed by a literal `9`).
LATIN1_ONLY = ["é", "\xa0", "\xff"]

# Ordered, so a range built from two indices i <= j is well formed.
RANGE_ALPHABET = "0123456789abcdefwxyz"

# ClassSetCharacter in v mode must avoid ClassSetSyntaxCharacter and the
# reserved double punctuators; letters and digits are unconditionally safe.
CLASS_SET_SAFE = list("abcxyzABXY019")

CONTROL_ESCAPE = list("fnrtv")
ASCII_LETTER = list("abcxyzABXYZ")

# The character each ControlEscape stands for, so the subject pool can contain
# something the pattern can actually match.
ESCAPE_LITERALS = {"f": "\f", "n": "\n", "r": "\r", "t": "\t", "v": "\v"}

# Property names that exist in every ICU build V8 ships with.
UNICODE_PROPERTIES = [
    "L",
    "Lu",
    "Ll",
    "N",
    "Nd",
    "P",
    "S",
    "Zs",
    "Alphabetic",
    "White_Space",
    "Script=Greek",
    "Script=Latin",
    "General_Category=Letter",
]

# Properties of strings: v-mode only, and the one \p{} form that can match
# more than a single character, so it runs the same string-valued machinery
# as `\q{}`.  Negating one is an early error, which is why \P{} draws from
# UNICODE_PROPERTIES alone.
UNICODE_PROPERTIES_OF_STRINGS = [
    "Basic_Emoji",
    "RGI_Emoji",
    "RGI_Emoji_Flag_Sequence",
    "RGI_Emoji_Modifier_Sequence",
    "RGI_Emoji_Tag_Sequence",
    "RGI_Emoji_ZWJ_Sequence",
    "Emoji_Keycap_Sequence",
]

# Characters mixed into a subject on top of the pattern's own literals, so a
# match has something to fail against and the two-byte / surrogate subject
# representations are reached even when the pattern is pure ASCII.
SUBJECT_NOISE = list("abcxyz019 \n") + ["é", "\U0001f0a1"]

# Probability that a subject character is drawn from the pattern's literals
# rather than SUBJECT_NOISE.  High, because drawing from the pattern is what
# makes a meaningful share of cases match at all (see gen_subject).
SUBJECT_LITERAL_SHARE = 0.75
SUBJECT_MAX_LEN = 10

# Probability that a case sets a nonzero lastIndex.
LAST_INDEX_SHARE = 0.3
