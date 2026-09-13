// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Coverage for the fixed-length-loop drain reductions in regexp-compiler.cc.
// A greedy fixed-length loop normally emits a char-by-char drain that retries
// the continuation at each retreated position. When the loop is provably
// atomic for its continuation (an AT_END assertion, a \b after a word-char
// body, or a body charset disjoint from the continuation's first character)
// the drain is dead: no retreat can turn the failed continuation into a match.
// The compiler then skips the drain and, for the unanchored search body, parks
// the position at the loop's greedy extent so the search resumes there instead
// of rescanning the run (O(n) instead of O(n^2)). Parking a sibling
// alternative's give-up would be unsound, so it is gated on an explicit
// ParkedGrant carried on the Trace and revoked by any backtrack retarget.
//
// All checks below are pure match-result assertions, valid in any tier;
// single-tier codegen exercises the native emission.

// Strict: asserts the full match array (length and every element), or null.
// An empty match leaked to the wrong position differs only in index, so pin
// that too when given.
function check(re, str, expected, index) {
  const result = re.exec(str);
  if (expected === null) {
    assertNull(result, `${re}.exec(${JSON.stringify(str)}) expected null`);
  } else {
    assertNotNull(result, `${re}.exec(${JSON.stringify(str)}) expected match`);
    assertEquals(expected.length, result.length,
                 `${re}.exec(${JSON.stringify(str)}) array length`);
    for (let i = 0; i < expected.length; i++) {
      assertEquals(expected[i], result[i],
                   `${re}.exec(${JSON.stringify(str)})[${i}]`);
    }
    if (index !== undefined) {
      assertEquals(index, result.index,
                   `${re}.exec(${JSON.stringify(str)}) index`);
    }
  }
}

// ---------------------------------------------------------------------------
// Drain omission for <class>+$ shapes (AtomicLoopKind::kAtEnd).
// A loop whose continuation is provably retreat-insensitive skips restoring
// the position to the loop entry, leaving it at the greedy extent. That is
// only sound when the continuation-failure backtrack rescans forward (the
// unanchored-search retry) or fails; a sibling that matches at the leftover
// position is excluded by the parked-position grant (issued only where the
// implicit search loop emits its body and revoked on any backtrack retarget).
// ---------------------------------------------------------------------------

// The drain omission must not leak the loop's advanced position to a sibling
// alternative: /b*$|/ on "bx" must match empty at index 0, not 1. The static
// give-up path is fenced by the parked-position grant (revoked at the sibling
// choice); the backtrack-stack path restores the position via the flush's
// undo frame.
check(/b*$|/, 'bx', [''], 0);         // empty sibling at index 0.
check(/c*$|c/, 'c9', ['c'], 0);       // 'c' sibling wins at index 0.
check(/\w*$|/, 'y:', ['']);
// Fuzzer-derived shape: the (){0} group is elided from the node graph but
// keeps a slot in the result array, and [8-a] spans the ASCII punctuation
// between digits and letters (matching the ':').
check(/[8-a]*(){0}$|/, ':x', ['', undefined], 0);

// Overlapping sibling: the `a` alternative can match inside the run the `a+`
// consumed, so the position must be at the choice entry, not the run end.
// Omission fires via the backtrack-stack branch here; the flush's undo frame
// restores the position for the sibling.
check(/x(a+$|a)/, 'xaab', ['xa', 'a']);
check(/pre(\d+$|\d)/, 'pre12x', ['pre1', '1']);

// Omission stays enabled for the unanchored search body (the case it exists
// for); these pin the results on the canonical class-plus-end shapes.
check(/\s+$/, '  x', null);
check(/\s+$/, 'a   ', ['   ']);
check(/\d+$/, '12x', null);
check(/\d+$/, 'a123', ['123']);
check(/[a-z]+$/, '9abc', ['abc']);

// Star shapes flush with cp_offset 0, taking the static give-up path: the
// search retry continues from the greedy extent instead of re-scanning the
// run. Starts inside the run are equivalent (same endpoint), and the extent
// itself provably fails, so results are unchanged. Indices pinned since a
// star loop can match empty anywhere.
check(/\s*$/, '  x', [''], 3);
check(/\s*$/, '  x   ', ['   '], 3);
check(/\s*$/, '   ', ['   '], 0);
check(/\d*$/, '12x34', ['34'], 3);
check(/\d*$/, '12x', [''], 3);

// Plus shapes carry the mandatory first character as a deferred cp advance;
// the static give-up path applies only under the uniform-prefix grant
// (ParkedGrant::kParkedUniformPrefix). A match must still be found past a
// failed run.
check(/\s+$/, '  x ', [' '], 3);
check(/\d+$/, '11x2', ['2'], 3);
check(/\d{2,}$/, '9x12', ['12'], 2);      // unrolled min=2 prefix.
check(/\d{2,}$/, '912x', null);
check(/\d{2,}$/, 'x9122', ['9122'], 1);
check(/a+$/, 'aax a', ['a'], 4);          // atom body.
check(/aa+$/, 'baaa', ['aaa'], 1);        // atom prefix, atom body.
check(/[a-z]+$/i, '9AbC', ['AbC'], 1);    // folded class, both sides.

// Prefixes NOT drawn from the loop class must not park: the character at
// the greedy extent can start a match. With an unconditional cp != 0 park,
// /x[ab]*$/ on "xxab" would consume the second "x" during the retry and
// miss the match at index 1.
check(/x[ab]*$/, 'xxab', ['xab'], 1);
check(/[0-3][0-9]*$/, '01x23', ['23'], 3);  // subset, not identical.
check(/\s\s*$/, 'a  x  ', ['  '], 4);       // distinct AST nodes.

// A back reference consumes input without a deferred cp advance; the flush
// at the back reference node fences the grant and its undo frame restores
// the position on give-up.
check(/(.)\1[0]*$/, 'yy0zz0', ['zz0', 'z'], 3);

// The scan-retry position-save skip in Trace::Flush is unsound with a consumed
// prefix in the trace (cp_offset_ != 0); the cp_offset_ == 0 guard excludes it.
// Without the guard /y|.[0]*$/ on ".9" wrongly matched ".9" at index 0 instead
// of "9" at index 1.
check(/y|.[0]*$/, '.9', ['9']);

// ---------------------------------------------------------------------------
// DrainMode::kRestoreOnly for disjoint continuations
// (AtomicLoopKind::kDisjoint). A loop whose body character set is
// disjoint from the continuation's first mandatory character set replaces the
// char-by-char drain with a single restore-to-entry: every retry position
// holds a body character the continuation cannot accept, so all retries --
// including the one at the loop entry -- are futile; only the position restore
// survives.
// ---------------------------------------------------------------------------

// Basic disjoint class/atom shapes.
check(/[abc]*d/, "abcd", ["abcd"], 0);
check(/[abc]*d/, "abcx", null);
check(/\d+[a-z]/, "123x", ["123x"], 0);
check(/\d+[a-z]/, "123 x", null);
check(/[ab]*c/, "aabbc!", ["aabbc"], 0);

// Overlapping sets stay on the full drain and must behave.
check(/[ab]*a/, "aab", ["aa"], 0);
check(/[abd]*d/, "abdd", ["abdd"], 0);

// The give-up must restore the position for a sibling alternative.
check(/([ab]+c|a)/, "abd", ["a", "a"], 0);

// Nested under another quantifier the restored give-up position is
// load-bearing: parking at the greedy extent would let the enclosing loop's
// continuation match at the wrong position.
check(/(?:[ab]+c)+d/, "abcabcd", ["abcabcd"], 0);
check(/(?:[ab]+c)+d/, "abcabd", null);
check(/(?:[abc]*d)+e/, "abcdabce", null);

// Captures: the FIRST(C) walk skips the leading STORE_POSITION.
check(/[ab]*(c+)d/, "abccd", ["abccd", "cc"], 0);
check(/[ab]*(c)/, "abc", ["abc", "c"], 0);

// A capture stored by the failed attempt must not leak into the sibling's
// result: group 1 is undefined when `.*` matches.
check(/(?:[ab]*(c)z|.*)/, "abcq", ["abcq", undefined], 0);

// Multi-element and atom bodies (the body set is the union over all
// elements; a retry position always holds the first element's character).
check(/(?:ab)*cd/, "ababcd", ["ababcd"], 0);
check(/(?:ab)*cd/, "ababcx", null);
check(/a+b/, "aaab", ["aaab"], 0);

// Ignore-case: class ranges are pre-folded and stay eligible; atoms fold at
// emission time, so /[a-c]*d/i takes the full drain -- both must behave.
check(/[a-c]*[d]/i, "aBcD", ["aBcD"], 0);
check(/[a-c]*d/i, "aBcD", ["aBcD"], 0);
check(/[a-c]*D/, "abcD", ["abcD"], 0);

// Unicode mode and two-byte subjects.
check(/[α]+x/u, "ααx", ["ααx"], 0);
check(/\d+[a-z]/u, "42q", ["42q"], 0);

// FIRST(C) of a multi-char atom is just its first character; a shared
// character later in the atom does not break disjointness.
check(/[abc]*dab/, "abcdab", ["abcdab"], 0);
check(/[abc]*da/, "abcda", ["abcda"], 0);

// Failures deeper in the continuation funnel through the same give-up.
check(/[abc]*de/, "abcdx", null);
check(/[abc]*de/, "abcde", ["abcde"], 0);

// Global scan across multiple matches.
assertEquals(["ab1", "cd2"], "ab1cd2".match(/[a-z]+\d/g));

// Zero-iteration entries: the extent attempt is the entry attempt.
check(/[abc]*d/, "xyzd", ["d"], 3);

// Long runs terminate quickly without a match.
check(/[a-z]+0/, "q".repeat(10000) + "!", null);

// ---------------------------------------------------------------------------
// DrainMode::kRetryAtEntry for <word-class>+\b shapes
// (AtomicLoopKind::kBoundary). A loop over word characters
// whose continuation starts with \b collapses the char-by-char drain into a
// single retry at the loop entry: interior retreat positions sit between two
// word characters where \b is false, so only the entry position (whose left
// neighbor is unknown) can turn a retry into a match.
// ---------------------------------------------------------------------------

// The entry retry must survive: the zero-iteration endpoint can match even
// though every interior retreat position is futile. A full drain omission
// would miss these matches.
check(/[ab]*\ba/, "abx", ["a"], 0);
check(/[ab]*\bz/, "z", ["z"], 0);
check(/[ab]*\bb/, "x ab", null);

// Basic hits and misses.
check(/\w+\b=/, "key=value", ["key="], 0);
check(/\w+\b=/, "abc def", null);
check(/[a-z]+\b-x/, "ab-x", ["ab-x"], 0);
check(/[ab]+\bc/, "abc", null);

// The give-up must restore the position for a sibling alternative.
check(/[ab]+\bz|ab/, "abq", ["ab"], 0);

// Nested under another quantifier the restored give-up position is
// load-bearing: parking at the greedy extent would let the continuation
// match at the wrong position.
check(/(?:[ab]+\b,)+c/, "ab,ab,c", ["ab,ab,c"], 0);
check(/(?:[ab]+\b,)+c/, "ab,abc", null);
check(/(?:\w+\b )+x/, "aa bb x", ["aa bb x"], 0);
check(/(?:[ab]+\b.)+q/, "ab,abq", null);

// Captures in the continuation after \b.
check(/\w+\b(=+)/, "ab==c", ["ab==", "=="], 0);

// Global scan across multiple matches.
assertEquals(["ab=", "cd="], "xx ab= cd=".match(/\w+\b=/g));

// Ignore-case class body stays eligible: the folded ranges are still word
// characters.
check(/[a-z]+\b=/i, "AbC=1", ["AbC="], 0);

// Atom bodies; under /i atoms take the unoptimized path but must behave
// identically.
check(/(?:ab)+\b=/, "abab=1", ["abab="], 0);
check(/(?:ab)+\b=/i, "aBab=1", ["aBab="], 0);

// \B between two word characters is true, so interior retries are useful;
// \B loops must not take the optimized path.
check(/[ab]+\Bc/, "abc", ["abc"], 0);

// Bodies containing non-word characters are ineligible: a retreat can move
// the boundary window onto a word/non-word edge.
check(/[a-z ]+\b!/, "a b !", null);
check(/[a-z ]+\b!/, "a b!", ["a b!"], 0);

// Under /iu, [k] folds to include U+212A KELVIN SIGN, which \b treats as a
// non-word character; the body is rejected and matching stays correct.
check(/[k]+\bx/iu, "Kx", null);
check(/[k]+\b=/iu, "kK=", ["kK="], 0);

// Two-byte subjects.
check(/\w+\b=/, "α ab= x", ["ab="], 2);

// Long runs terminate quickly without a match.
check(/\w+\b=/, "a".repeat(10000) + "!", null);

// lastIndex bookkeeping with a sticky-ish scan; no match anywhere here
// because every candidate entry has a word character on its left.
const g = /[ab]*\ba/g;
g.lastIndex = 0;
assertNull(g.exec("xxabx"));

// ---------------------------------------------------------------------------
// Negated class bodies (AppendClassRangesMatchSet). A negated class body such
// as [^"] is materialized as the complement of its listed ranges, so common
// tokenizer shapes reach the disjoint/boundary reductions instead of bailing.
// ---------------------------------------------------------------------------

// Negated body, disjoint continuation (kRestoreOnly / kDisjoint).
check(/"[^"]*"/, 'a"bc"d', ['"bc"'], 1);
check(/"[^"]*"/, 'x"abc', null);
check(/'[^']*'/, "x'hi'y", ["'hi'"], 1);
check(/<[^>]+>/, 'a<b>c', ['<b>'], 1);
check(/<[^>]+>/, 'a<bc', null);
check(/[^0-9]+0/, 'ab0', ['ab0'], 0);
check(/[^0-9]+0/, 'abc', null);
check(/[^"\\]+"/, 'ab"', ['ab"'], 0);

// Negated continuation first character (\W is a negated class).
check(/\w+\W/, 'abc!', ['abc!'], 0);
check(/\w+\W/, 'abc', null);
check(/[a-z]+[^a-z]/, 'abc1', ['abc1'], 0);

// Negated body under ignore-case: [^a] excludes a and A.
check(/[^a]+a/, 'bcba', ['bcba'], 0);
check(/[^a]+a/i, 'BCA', ['BCA'], 0);

// Unicode and two-byte subjects.
check(/[^α]+α/u, 'abαc', ['abα'], 0);

// The give-up must restore the position for a sibling alternative; a leaked
// extent would let the sibling match at the wrong place.
check(/([^"]+"|z)/, 'abz', ['z', 'z'], 2);
check(/([^"]+"|z)/, 'ab"', ['ab"', 'ab"'], 0);

// Nested under an enclosing quantifier the restore is load-bearing: parking
// at the greedy extent would match the continuation at the wrong position.
check(/(?:"[^"]*")+;/, '"a""b";', ['"a""b";'], 0);
check(/(?:"[^"]*")+;/, '"a""b"x', null);

// Global scan across negated-body matches.
assertEquals(['"a"', '"b"'], '"a" x "b"'.match(/"[^"]*"/g));

// Zero-iteration entry: the extent attempt is the entry attempt.
check(/[^0-9]*0/, 'xyz0', ['xyz0'], 0);
check(/[^0-9]*0/, '0', ['0'], 0);

// The parked search over a negated body is O(n): a long non-matching run is
// abandoned in one step (would time out at O(n^2)).
check(/[^\n]+\n/, 'a'.repeat(1 << 20), null);
assertEquals((1 << 20) + 1,
             /[^\n]+\n/.exec('a'.repeat(1 << 20) + '\n')[0].length);

// ---------------------------------------------------------------------------
// Parked-position dispatch on kDisjoint and kBoundary loops
// (LoopChoiceNode::atomic_loop_kind, ChooseFixedLengthLoopDrainMode). Under a parked
// grant the give-up leaves the position at the loop's greedy extent and the
// implicit search resumes from there instead of rescanning the run. The cases
// below pin the index-sensitive hazards.
// ---------------------------------------------------------------------------

// A sibling alternative must match at the original position, not at a
// leaked extent: the grant is revoked on any backtrack retarget.
check(/[ab]*c|/, "abx", [""], 0);
check(/\w*\b=|/, "ab!", [""], 0);
check(/[ab]+c|/, "abx", [""], 0);

// Non-uniform prefixes must not park: the skipped restart could consume the
// extent character that starts the real match.
check(/x[ab]*c/, "xxabc", ["xabc"], 1);
check(/x[ab]*$/, "xxab", ["xab"], 1);

// Window-shift hazard: the consumed prefix is not drawn from the loop
// source, so parking would skip start positions where the continuation's
// window differs.
check(/y|.[0]*!/, ".9!", ["9!"], 1);
check(/y|.[0]*!/, ".9", null);

// The parked search resumes at the extent; a viable restart there or later
// must still be found.
check(/[abc]*d/, "abxd", ["d"], 3);
check(/[ab]+c/, "aabxabc", ["abc"], 4);
check(/\w+\b=/, "ab cd=", ["cd="], 3);
check(/\w*\b=/, "ab =x", null);

// Assertion-skipping FIRST(C): \b followed by a disjoint character makes
// the loop kDisjoint, so no entry retry is needed and any grant parks.
check(/\w*\b=/, "ab=c", ["ab="], 0);
check(/\w*\b=/, "=x", null);
check(/\w+\b=/, "=x", null);
check(/[ab]*\b\b=/, "ab=", ["ab="], 0);

// FIRST(C) overlapping the body stays kBoundary and keeps its entry retry.
check(/[ab]*\ba/, "abx", ["a"], 0);
check(/[ab]*\bb/, "x ab", null);

// Trailing assertions with no consumed character stay on the boundary rule.
check(/\w*\b$/, "ab", ["ab"], 0);

// A leading \b does not break the uniform prefix: the grant walk skips it,
// so /\b\w+\b/ reaches the nonempty-uniform-prefix grant and parks its
// boundary give-up (the mandatory first word character proves the entry
// retry futile). The match is unchanged and long runs stay O(n).
check(/\b\w+\b/, "!!!abc!!!", ["abc"], 3);
check(/\b\w+\b=/, "!! key=val", ["key="], 3);
assertEquals(["the", "quick", "brown"], "the quick brown".match(/\b\w+\b/g));
check(/\b\w+\b=/, "!" + "a".repeat(1 << 20) + "!", null);

// Inside an enclosing quantifier there is no grant; the restore is
// preserved and parking must not occur.
check(/(?:[abc]*d)+e/, "abcdabce", null);
check(/(?:\w+\b,)+x/, "ab,cd,x", ["ab,cd,x"], 0);

// Global scans across parked give-ups.
assertEquals(["a1", "b2", "c3"], "a1 b2 c3".match(/[a-z]+\d/g));
assertEquals(["ab=", "cd="], "ab= cd=".match(/\w*\b=/g));

// Captures across parked give-ups.
check(/(\w+)\b=(\d)/, "ab cd=7", ["cd=7", "cd", "7"], 3);

// A parked give-up can land exactly at the subject end (impossible for
// AT_END shapes); the search-retry re-entry must bounds-check its reload.
check(/\w+\b=/, "dddd", null);
check(/[abc]*d/, "abc", null);
check(/[abc]*de/, "abcd", null);
check(/\w+\b=/, "a", null);

// Zero-width-matchable search bodies have a preload width of zero; the
// parked landing must not emit a zero-width load.
check(/\w*\b$/, "ab", ["ab"], 0);
check(/\w*\b$/, "", null);

// The parked search is O(n): a long run is abandoned in one step. These
// would time out at O(n^2).
const run = "a".repeat(1 << 20);
check(/[a-z]+0/, run + "!", null);
check(/\w+\b=/, run + "!", null);
check(/\w*\b=/, run + "!", null);
assertEquals((1 << 20) + 1, /[a-z]*0/.exec(run + "0")[0].length);

// A parked atomic loop that is one alternative of a search-body choice must
// not skip input positions where an EARLIER alternative can still match: the
// loop analysis proves the loop fails at the skipped positions but says
// nothing about the sibling.  Without a choice-local grant these matched at
// the wrong place or not at all. Covers all three parked kinds.
check(/:|\w*\s/, "c:", [":"], 1);            // kDisjoint sibling.
check(/[b]|\d*y/, "0b", ["b"], 1);
check(/z|[abc]*d/, "abz", ["z"], 2);
check(/b|a*$/, "aab", ["b"], 2);             // kAtEnd sibling.
check(/x|\s*$/, "  x", ["x"], 2);
check(/,|\w+\b;/, "ab,", [","], 2);          // kBoundary sibling.
// The winning sibling is a later alternative, past the loop.
check(/\d*x|y/, "aay", ["y"], 2);
// Two-byte subject.
check(/é|c*x/, "cé", ["é"], 1);
// Captures and global scans across the corrected give-up.
check(/(:)|(\w*)\s/, "ab:", [":", ":", undefined], 2);
assertEquals(["1", "2"], "a1b2".match(/\d|[a-z]*!/g));
// The loop as the FIRST alternative with an empty sibling still matches the
// empty at the start position (the sibling wins where it always did).
check(/[ab]*c|/, "abx", [""], 0);

// The exception to choice-locality: the grant may cross a choice into an
// alternative whose siblings all assert start-of-input.  Such a sibling
// matches only at position 0, which a park never skips, so no match can be
// lost.  This is the String.trim shape /^\s*|\s*$/.
assertEquals("ab", "   ab  ".replace(/^\s*|\s*$/g, ""));
assertEquals("", "     ".replace(/^\s*|\s*$/g, ""));
check(/^\s*|\s*$/, "  ab  ", ["  "], 0);
assertEquals(["  ", "  ", ""], "  ab  ".match(/^\s*|\s*$/g));
// All three parked kinds behind a start-anchored sibling.
check(/^x|[ab]*c/, "abx abc", ["abc"], 4);       // kDisjoint.
check(/^b|a*$/, "aab", [""], 3);                 // kAtEnd.
check(/^;|\w+\b=/, "ab cd=", ["cd="], 3);        // kBoundary.
// Two-byte subject.
check(/^é|c*x/, "ccy ccx", ["ccx"], 4);
// A capture wrapping the anchor does not unanchor the sibling.
check(/(^x)|[ab]*c/, "abx abc", ["abc", undefined], 4);
// Several anchored siblings are as safe as one.
check(/^x|^y|\s*$/, "  z  ", ["  "], 3);
// The parked alternative first: its backtrack is retargeted, so it cannot
// carry the grant, but results are unchanged.
check(/\s*$|^x/, "  z  ", ["  "], 3);
// A multiline ^ matches after any newline, including inside a skipped run;
// it must stay a barrier (the drain finds the match here).
check(/^b|[a\n]*$/m, "a\nb", ["a"], 0);

// With all siblings anchored the search is O(n) again.
const pad = "x" + " ".repeat(1 << 20) + "x";
assertEquals(2 + (1 << 20), /^y|\s*$/.exec(pad).index);

// ---------------------------------------------------------------------------
// Negative coverage: shapes that must NOT be treated as atomic. Each uses a
// subject where a lost drain retry (or an unsound park) changes the result.
// ---------------------------------------------------------------------------

// \B is true between two word characters, so interior retries can succeed.
// /[ab]+\Bb/ on "ab" only matches via the drain: at the greedy extent \B is
// false (end of input), one retreat puts it between 'a' and 'b'.
check(/[ab]+\Bb/, "ab", ["ab"], 0);
check(/\w+\Bc/, "abc", ["abc"], 0);
check(/\d+\B\d/, "123", ["123"], 0);

// Multiline $ (before a \n) can become true after retreating: on "a\nax"
// the greedy extent sits before 'x' where $/m is false, and only the drain
// retry before the '\n' matches.
check(/[a\n]*$/m, "a\nax", ["a"], 0);
check(/[^b]+$/m, "aa\nccb", ["aa"], 0);
// Multiline $ before a continuation character: retreating to the newline can
// succeed.
check(/[^x]*$[^]y/m, "a\nyy", ["a\ny"], 0);

// Lookarounds in the continuation depend on the position window.
check(/\d+(?=x)/, "123x", ["123"], 0);
check(/\d+(?=3)/, "123", ["12"], 0);       // retry inside the run matches.
check(/\d+(?!\d)/, "123", ["123"], 0);
check(/[ab]+(?<=b)/, "aba", ["ab"], 0);    // retreat changes the lookbehind.

// A capture inside the loop body must reflect the last successful iteration
// after the drain settles, not the greedy extent.
check(/(?:(\d)x)+$/, "1x2x", ["1x2x", "2"], 0);
check(/([ab])+c/, "abac", ["abac", "a"], 0);

// Back references: in the continuation the accepted set is dynamic, so no
// disjointness proof; in the body the match set is unknown.
check(/(x)[ab]*\1/, "xabx", ["xabx", "x"], 0);
check(/(a)\1+$/, "baaa", ["aaa", "a"], 1);
check(/(.)[bc]*\1/, "abca", ["abca", "a"], 0);

// Group modifiers: MODIFY_FLAGS changes what the body matches; the analysis
// must use the loop's own flag context.
check(/(?i:[k])+$/u, "kK", ["kK"], 0);
check(/(?i:a+)$/, "aA", ["aA"], 0);
check(/x(?i:[ab]+)c/, "xaBc", ["xaBc"], 0);

// Guarded alternatives (from interval quantifiers) keep the full drain.
check(/(?:a{1,4})*b/, "aaab", ["aaab"], 0);

// Zero-iteration star loops at each kind: the extent attempt IS the entry
// attempt and must still fail/match correctly.
check(/[b]*$/, "x", [""], 1);              // kAtEnd.
check(/[ab]*c/, "c", ["c"], 0);            // kDisjoint.
// kBoundary with zero iterations: \b holds at the loop entry (non-word left
// neighbor), so the kept entry retry must find this match.
check(/\w*\bx/, " x", ["x"], 1);

// ---------------------------------------------------------------------------
// Multi-code-unit body: the park (kOmit) is unsound for a body wider than one
// code unit. A restart misaligned with the body width consumes past the old
// greedy extent and can stop on a character the continuation accepts, so a
// parked give-up would skip a start position that matches. The park is gated
// on a single-unit body; wider loops fall back to a restore mode (which
// restores the position and stays correct). These would all mismatch if the
// park fired, so they pin the leftmost match and index.
// ---------------------------------------------------------------------------
check(/(?:aa)*c/, "aaaaac", ["aaaac"], 1);   // kDisjoint, greedy misalign.
check(/(?:aa)+c/, "aaaaac", ["aaaac"], 1);
check(/(?:[ab]a)*c/, "aaaaac", ["aaaac"], 1);
check(/(?:aa)*$/, "aaa", ["aa"], 1);         // kAtEnd, odd length.
check(/(?:aa)+\b/, "aaaaa aa", ["aaaa"], 1); // kBoundary, odd run.
check(/(?:abc)+d/, "abcabcabd", null);       // width 3, no match.

// ---------------------------------------------------------------------------
// kTotal: the continuation always succeeds at the greedy extent (see
// AtomicLoopKind::kTotal), so the drain is dead. Covers a bare tail loop, a
// trailing optional, and the leading loop of an adjacent nullable chain.
// ---------------------------------------------------------------------------
// Bare ACCEPT continuation: a tail loop with no trailing assertion at all.
check(/\w+/, '  abc  ', ['abc'], 2);
check(/\w+/, '!!!', null);
check(/(\S+)/, '  hi there', ['hi', 'hi'], 2);
check(/([^;]*)/, 'a=b;c', ['a=b', 'a=b'], 0);
check(/[a-z]+/, 'ABCabc', ['abc'], 3);
assertEquals(['  ', '   '], 'aa  bb   cc'.match(/\s+/g));
// A capture in the tail-loop body still records the last iteration.
check(/(\d)+/, 'x123', ['123', '3'], 1);
// Long non-matching run stays linear.
check(/\d+/, 'a'.repeat(1 << 20), null);
// Trailing optional / adjacent nullable chains.
check(/\w+;?/, "abc;", ["abc;"], 0);
check(/\w+;?/, "abc", ["abc"], 0);
check(/\w+;?/, "  abc; ", ["abc;"], 2);
check(/\w+(?:;|,)?/, "abc,", ["abc,"], 0);
// Adjacent nullable loops: the leading loop's continuation is the trailing
// loop chained to ACCEPT, which is nullable, so it too is kTotal.
check(/(\d*)(\D*)/, "12ab", ["12ab", "12", "ab"], 0);
check(/(\d*)(\D*)/, "ab", ["ab", "", "ab"], 0);
check(/(\d*)(\D*)/, "12", ["12", "12", ""], 0);
check(/(\d*)(\D*)/, "", ["", "", ""], 0);
check(/a*b*/, "aabb", ["aabb"], 0);
check(/a*b*/, "ba", ["b"], 0);
check(/a*b*/, "", [""], 0);
check(/(\w+)(\s*)/, "hi  x", ["hi  ", "hi", "  "], 0);
check(/\w*(\d|)/, "ab3", ["ab3", ""], 0);

// Priority is preserved: making the loop atomic must not reorder the
// continuation's own alternatives. The greedy loop consumes the char the
// later alternative would have matched, so the empty alternative wins.
check(/\w+(a|)/, "test", ["test", ""], 0);
check(/\w+(x|)/, "testx", ["testx", ""], 0);

// Negative cases: the continuation is NOT total, so kTotal must not fire (a
// wrong classification would park/omit the drain and change the result).
// A `+` / mandatory continuation is not nullable (min-count guard on the exit
// alternative, or a mandatory text node).
check(/\w+\d+/, "ab12", ["ab12"], 0);
check(/\w+\d+/, "ab", null);
check(/\w+\d/, "ab1", ["ab1"], 0);
check(/\w+\d/, "ab", null);
// A positive lookahead consumes nothing but can fail, so it is not total.
check(/\w+(?=;)/, "abc;", ["abc"], 0);
check(/\w+(?=;)/, "abc", null);
// A negative lookaround continuation is not a plain disjunction and forces a
// retreat, so it must keep its drain.
check(/\w+(?!;)/, "abc;", ["ab"], 0);
check(/\w+(?!;)/, "abc", ["abc"], 0);
// A backreference continuation is not nullable.
check(/(a+)\1/, "aaaa", ["aaaa", "aa"], 0);
check(/(a+)\1/, "aaa", ["aa", "a"], 0);

// kTotal reaches through infallible actions, including a matched positive
// lookahead: recursing into the lookahead body reports success only when the
// body (and the outer continuation after it) is itself infallible. So a loop
// before or inside a nullable lookahead is total, while a lookahead that can
// fail keeps its drain. A negative lookaround always keeps its drain.
check(/\w+(?=a*)/, "abc", ["abc"], 0);        // nullable lookahead: total.
check(/\w+(?=a*)/, "abaa", ["abaa"], 0);
check(/[ab]*(?=c*)/, "abcc", ["ab"], 0);
check(/(?=a*)x/, "yx", ["x"], 1);             // loop inside the lookahead.
check(/\w+(?=a)/, "testa", ["test"], 0);      // mandatory lookahead: not total.
check(/\w+(?=a)/, "test", null);
check(/\w+(?!a)/, "testa", ["testa"], 0);     // negative lookaround: not total.
check(/\w+(?!a)/, "testb", ["testb"], 0);
check(/\w+(?=a*)\d/, "ab5", ["ab5"], 0);      // outer continuation not total.
check(/\w+(?=a*)z/, "ab", null);
check(/x(?=(a*))y/, "xaay", null);            // capture inside lookahead.
check(/x(?=(a*))y/, "xy", ["xy", ""], 0);
