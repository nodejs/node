// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Coverage for the first-character rejection bitset computed in
// Compiler::Assemble and tested in RegExpExecInternal. The bitset marks
// characters that cannot begin a match, so it must be the complement of a
// superset of FIRST(pattern): rejecting a character that can start a match
// would turn a match into a null result. These tests pin patterns whose shape
// makes the 4-character mask degenerate, which is where the bitset carries the
// rejection.

// Runs each pattern twice, because the two execs take different code paths:
// the one that compiles is filtered by RegExpData::QuickCheckRejects, later
// ones by RegExpExecInternal.
function check(re, str, expected, index) {
  for (let i = 0; i < 2; i++) {
    const lastIndex = re.lastIndex;
    const result = re.exec(str);
    const where = `${re}.exec(${JSON.stringify(str)}) (run ${i})`;
    if (expected === null) {
      assertNull(result, where);
    } else {
      assertNotNull(result, where);
      assertEquals(expected, result[0], `${where}[0]`);
      if (index !== undefined) assertEquals(index, result.index, where);
    }
    re.lastIndex = lastIndex;
  }
}

// Asserts that the filter is built and rejects |c|. Exec results alone cannot
// show this: a missing filter costs performance, not correctness, so a pattern
// whose filter silently disappears still passes every check() above.
function assertQuickCheckRejects(re, c) {
  re.exec('');  // Compile.
  assertTrue(%RegexpQuickCheckRejects(re, c), `${re} should reject '${c}'`);
}

// An alternation over dissimilar leading characters: no bit is common to all of
// them, so the mask erodes while the bitset stays exact.
const punctuator = /--|\+\+|=>|\.{3}|[?~,:;[\](){}]/y;
for (const op of ['--', '++', '=>', '...', '?', '~', ',', ':', ';', '[', ']',
                  '(', ')', '{', '}']) {
  punctuator.lastIndex = 0;
  check(punctuator, op, op, 0);
}
for (const nonOp of ['a', 'Z', '0', ' ', '\t', 'é', '中']) {
  punctuator.lastIndex = 0;
  check(punctuator, nonOp, null);
}
punctuator.lastIndex = 0;
assertQuickCheckRejects(punctuator, 'a');

// Two leading characters differing in several bits (0x60 vs 0x7d).
const template = /[`}](?:[^`\\$]|\\[^]|\$(?!\{))*(`|\$\{)?/y;
for (const s of ['`abc`', '}abc`', '`', '}']) {
  template.lastIndex = 0;
  check(template, s, s, 0);
}
template.lastIndex = 0;
check(template, 'abc', null);
template.lastIndex = 0;
assertQuickCheckRejects(template, 'a');

// A negated class leads the pattern: the bitset builder materializes the
// complement, so every character outside the negated set must still match.
const notQuote = /"[^"]*"/;
check(notQuote, '"hi"', '"hi"', 0);
check(notQuote, 'x"hi"', '"hi"', 1);
check(notQuote, 'nope', null);

const jsxText = /[^<>{}]+/y;
for (const s of ['abc', ' ', 'é', '中']) {
  jsxText.lastIndex = 0;
  check(jsxText, s, s, 0);
}
for (const s of ['<', '>', '{', '}']) {
  jsxText.lastIndex = 0;
  check(jsxText, s, null);
}

// Case-insensitive patterns: a bitset built from the pattern character alone
// would wrongly reject the other case.
const ci = /^hsl/i;
check(ci, 'hsl(1)', 'hsl', 0);
check(ci, 'HSL(1)', 'HSL', 0);
check(ci, 'HsL(1)', 'HsL', 0);
check(ci, 'rgb', null);

// Latin-1 case pairs fold too.
check(/^ä/i, 'Ä!', 'Ä', 0);
check(/^ä/i, 'x', null);

// Characters whose unicode case-fold partner is ASCII (U+212A KELVIN -> k/K,
// U+017F LONG S -> s/S) must not cause the ASCII partner to be rejected.
// Escaped: both are hard to tell from their ASCII partner in source.
check(/^\u212a/iu, 'k', 'k', 0);
check(/^\u212a/iu, 'K', 'K', 0);
check(/^\u212a/iu, 'x', null);
check(/^\u017f/iu, 's', 's', 0);
check(/^\u017f/iu, 'q', null);

// Non-unicode /i folds through a different path than /iu above, and a sticky
// pattern reaches the leading atom without an anchor in front of it.
const ciSticky = /k/iy;
check(ciSticky, 'K', 'K', 0);
ciSticky.lastIndex = 0;
check(ciSticky, 'k', 'k', 0);
ciSticky.lastIndex = 0;
check(ciSticky, 'x', null);
check(/^é/i, 'É', 'É', 0);
check(/^É/i, 'é', 'é', 0);
check(/^é/i, 'e', null);

// An atom whose case variants all fall outside latin-1 yields an empty set on
// a one-byte compile.  That is smaller than FIRST, not larger, so the walk
// gives up rather than describing the node as matching nothing.
check(/^\u03a9/i, '\u03a9', '\u03a9', 0);
check(/^\u03a9/i, '\u03c9', '\u03c9', 0);
check(/^\u03a9/i, 'x', null);

// Two-byte subjects are never filtered, but must still behave.
check(/^[0-9]/, '中5', null);
check(/[0-9]/, '中5', '5', 1);

// A regexp that sees a two-byte subject first compiles twice, and only the
// later one-byte compilation fills in the filters. Results must not depend on
// which encoding came first.
const encodingSwitch = /^[a-c]/;
check(encodingSwitch, '中b', null);
check(encodingSwitch, 'zzz', null);
check(encodingSwitch, 'bbb', 'b', 0);
assertQuickCheckRejects(encodingSwitch, 'z');

// A leading lookahead constrains the first character; a leading negative
// lookahead does not, but its continuation still does.
check(/(?=[ab])[a-c]/, 'c', null);
check(/(?=[ab])[a-c]/, 'b', 'b', 0);
check(/(?![ab])[a-c]/, 'c', 'c', 0);
check(/(?![ab])[a-c]/, 'a', null);
// The lookaround itself constrains nothing, but the continuation after it
// does, so the filter comes from [a-c].  Anchored, since the implicit .*
// prefix of an unanchored pattern floods the set anyway.
check(/^(?!x)[a-c]/, 'c', 'c', 0);
check(/^(?!x)[a-c]/, 'z', null);
assertQuickCheckRejects(/^(?!x)[a-c]/, 'z');
assertQuickCheckRejects(/(?!x)[a-c]/y, 'z');

// A class that cannot match latin-1 compiles to a backtrack on a one-byte
// compile.  A backtrack never matches, so it contributes the empty set and the
// walk keeps the continuation's; treating it as unknown would lose the filter.
check(/^(?=[\u{1F600}])[abc]/uy, 'a', null);
check(/^(?=[\u{1F600}])[abc]/uy, 'z', null);
assertQuickCheckRejects(/^(?=[\u{1F600}])[abc]/uy, 'z');

// A *nullable* lookahead body consumes nothing, so the continuation supplies
// the first character; taking the body's set alone would reject it.
check(/^(?=a*)b/, 'b', 'b', 0);
check(/^(?=[a]?)c/, 'c', 'c', 0);
check(/^(?=(?:))d/, 'd', 'd', 0);
check(/^(?=a*)(?=b*)c/, 'c', 'c', 0);
check(/^(?=\b)x/, 'x', 'x', 0);
check(/^(?=$|a)a/, 'a', 'a', 0);

// A lookbehind matches to the left of the position, so it says nothing about
// the character consumed at it; the continuation does.
check(/(?<=x)[a-c]/, 'xb', 'b', 1);
check(/(?<=x)[a-c]/, 'xz', null);
check(/(?<![0-9])[a-c]/, 'b', 'b', 0);
check(/(?<![0-9])[a-c]/, 'z', null);
assertQuickCheckRejects(/(?<=x)[a-c]/y, 'z');
assertQuickCheckRejects(/(?<![0-9])[a-c]/y, 'z');

// Backreferences are not statically known.
check(/(a|b)\1/, 'bb', 'bb', 0);
check(/(.)\1/, 'éé', 'éé', 0);

// A flag modifier group changes which characters match downstream, so the set
// must not be built under the outer flags.
check(/^(?i:K)b/, 'kb', 'kb', 0);
check(/^(?i:ä)x/, 'Äx', 'Äx', 0);

// A modifier group with a *branching* body shares one flag-restore node
// between its alternatives, so emission leaves the group's inner flags behind.
// The set must be built under the pattern's own flags regardless: reading the
// inner ones here drops /i and rejects the lowercase leading atom.
check(/^K(?-i:(?:x|y))/i, 'ky', 'ky', 0);
check(/^K(?-i:(?:x|y))/i, 'kx', 'kx', 0);
check(/^K(?-i:(?:x|y))/i, 'kz', null);
check(/^ä(?-i:(?:x|y))/i, 'Äy', 'Äy', 0);
check(/^K(?-i:(?!q))x/i, 'kx', 'kx', 0);
check(/^k(?i:(?:X|Y))/, 'kY', 'kY', 0);
check(/^k(?i:(?:X|Y))/, 'Ky', null);

// A quantified nullable body inserts an empty-match check, which conditionally
// fails rather than consuming.
check(/^(?:a?)*b/, 'b', 'b', 0);

// Supplementary code points are matched via an unanchored advance.
check(/^./su, '\u{1F0A1}', '\u{1F0A1}', 0);

// An empty-matching pattern accepts at any position, so nothing may be
// rejected.
check(/a*/y, 'zzz', '', 0);
check(/(?:)/y, 'zzz', '', 0);

// Sticky patterns filter at lastIndex rather than 0.
const sticky = /[0-9]+/y;
sticky.lastIndex = 2;
check(sticky, 'ab123', '123', 2);
sticky.lastIndex = 0;
check(sticky, 'ab123', null);
assertEquals(0, sticky.lastIndex);

// Anchored-at-end and multiline patterns must not be filtered against
// position 0 alone.
check(/^a$/m, 'b\na', 'a', 2);
check(/^[0-9]$/m, 'x\n7', '7', 2);

// Unanchored patterns can match anywhere, so no first character constrains
// them.
check(/[0-9]+/, 'abc99', '99', 3);
check(/foo/, 'xxfoo', 'foo', 2);

// The filter must survive the pattern being recompiled to a different one.
const recompiled = /^a+/;
check(recompiled, 'abc', 'a', 0);
check(recompiled, 'zzz', null);
recompiled.compile('^z+');
check(recompiled, 'zzz', 'zzz', 0);
check(recompiled, 'abc', null);
