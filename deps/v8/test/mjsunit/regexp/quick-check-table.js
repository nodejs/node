// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Coverage for the first-character membership table emitted by
// Node::EmitQuickCheck when an alternative's single-character quick check is
// approximate (see TryBuildFirstCharacterTable in regexp-compiler.cc). The
// table rejects non-members exactly; it must be a superset of the matchable
// first characters (false positives allowed, false negatives not), so these
// pin that the optimized path returns the same results as the mask path.

function check(re, str, expected, index) {
  const result = re.exec(str);
  if (expected === null) {
    assertNull(result, `${re}.exec(${JSON.stringify(str)})`);
  } else {
    assertNotNull(result, `${re}.exec(${JSON.stringify(str)})`);
    assertEquals(expected, result[0], `${re}.exec(${JSON.stringify(str)})[0]`);
    if (index !== undefined) {
      assertEquals(index, result.index, `${re}.exec(${JSON.stringify(str)})`);
    }
  }
}

// A js-tokens-style operator alternation: the operator group's merged quick
// check is approximate, so its members are dispatched via a membership table.
// A single-character punctuator (last alternative) must still match, and a
// non-member must still be rejected. Sticky, so there is no implicit search
// loop pre-populating the shared bm_info -- each alternative gets its own
// precise first-character set (cf. the unanchored variant below).
const P =
    /(?:&&|\|\||\?\?|[+\-%&|^]|\*{1,2}|<{1,2}|>{1,3}|!=?|={1,2})=?|[?~,:;[\](){}]/y;
for (const [s, want] of [[",", ","], [")", ")"], [";", ";"], ["^", "^"],
                         ["%", "%"], [">>>=", ">>>="], ["&&", "&&"]]) {
  P.lastIndex = 0;
  check(P, s, want, 0);
}
for (const s of ["x", "9", "a", " "]) {
  P.lastIndex = 0;
  check(P, s, null);
}

// The same alternation unanchored. Now an implicit search loop wraps the whole
// regexp and pre-fills a shared Boyer-Moore lookahead across all alternatives;
// the per-node table walk must recover each alternative's own set rather than
// that union, or it would emit a table no more selective than the mask.
const PU =
    /(?:&&|\|\||\?\?|[+\-%&|^]|\*{1,2}|<{1,2}|>{1,3}|!=?|={1,2})=?|[?~,:;[\](){}]/;
check(PU, "abc%de", "%", 3);
check(PU, "xx,yy", ",", 2);
check(PU, "helloworld", null);

// A scattered class as a leading alternative: the mask is approximate, so the
// table drives the check. Members match, non-members are rejected, and an
// unanchored search still finds a later member.
check(/[+\-%&|^]/, "abc%de", "%", 3);
check(/[+\-%&|^]/, "abcde", null);
check(/[+\-%&|^]x|zz/, "y%x", "%x", 1);

// Negative: a choice with an omnivorous [\s\S] alternative lowers to an
// UnanchoredAdvance whose first-character set is unconstrained. The quick
// check must not fire a table that excludes any character (would be a false
// negative), so this must match through the [\s\S] branch.
check(/(?:%|[\s\S])x/u, "ax", "ax", 0);
check(/(?:%|[\s\S])x/u, "%x", "%x", 0);
check(/(?:%|[\s\S])x/u, "zy", null);

// A wide first-character set falls back to the mask when the mask is at least
// as discriminating; results are unchanged.
check(/[\x20-\x7e]y|zz/, "Ay", "Ay", 0);
check(/[\x20-\x7e]y|zz/, "\ty", null);

// Case-independence. The table walk (TextNode::FillInBMInfo sets every
// GetCaseIndependentLetters result) and the mask (GetQuickCheckDetails merges
// the same set) fill the set differently, so exercise both the explicit i flag
// and a mid-pattern (?i:...) modifier group.
check(/(?:[+\-%&|^]|kg)x/i, "KGx", "KGx", 0);
check(/(?:[+\-%&|^]|kg)x/i, "%x", "%x", 0);
check(/(?:[+\-%&|^]|kg)x/i, "abx", null);
check(/(?i:[+\-%&|^]|kg)x/, "KGx", "KGx", 0);
check(/(?i:[+\-%&|^]|kg)x/, "abx", null);

// One-byte mod-128 aliasing: 'i' (0x69) aliases 'é' (0xe9) modulo 128, so the
// table accepts it and the subsequent full check must still reject it.
check(/[éü]x|zz/, "éx", "éx", 0);
check(/[éü]x|zz/, "ix", null);
check(/[éü]x|zz/, "zz", "zz", 0);

// A widely scattered two-byte class: the table is indexed modulo 128, so a
// non-member aliasing a member's low seven bits must still be rejected by the
// subsequent full check.
check(/[؀ॿ]/, "؀", "؀", 0);  // A member.
check(/[؀ॿ]/, "ॿ", "ॿ", 0);  // A member.
check(/[؀ॿ]/, "x", null);
check(/[؀ॿ]/, "ڀ", null);  // Aliases a member modulo 128.
check(/[؀ॿ]/, "ۿ", null);  // Aliases a member modulo 128.

// Regression guard for a miscompile. [ƀƁҀ] folds to a range that 'Ѐ' sits in
// while also aliasing a member modulo 128. Were the table to fire here it would
// have to clear the untested mask, or the downstream fold drops the range's
// discriminating compare and matches 'Ѐ'. The selectivity guard keeps this
// clustered class on the mask path, so 'Ѐ' is rejected outright.
check(/[ƀƁҀ]|ab/, "Ѐ", null);
check(/[ƀƁҀ]|ab/, "ƀ", "ƀ", 0);
check(/[ƀƁҀ]|ab/, "ab", "ab", 0);

// The table gates a single alternative, not the choice: the [+\-%&|^] branch
// gets its own table and on a miss falls through to the sibling \1 branch, so a
// character the table excludes ('a') must still match via the back-reference.
check(/(.)(?:[+\-%&|^]|\1)x/, "a%x", "a%x", 0);
check(/(.)(?:[+\-%&|^]|\1)x/, "aax", "aax", 0);

// Read-backward position: inside a lookbehind the table is tested at a negative
// cp_offset. A member matches and a non-member is rejected there too.
check(/(?<=[+\-%&|^]a)b/, "+ab", "b", 2);
check(/(?<=[+\-%&|^]a)b/, "xab", null);

// not_at_start == true: the lookahead is keyed on it and TextNode flips it past
// a text node, so a quick check reached only after a leading atom reads the
// other slot. Members match, non-members are rejected.
check(/x(?:[+\-%&|^]|ab)/, "x%", "x%", 0);
check(/x(?:[+\-%&|^]|ab)/, "xab", "xab", 0);
check(/x(?:[+\-%&|^]|ab)/, "xy", null);
