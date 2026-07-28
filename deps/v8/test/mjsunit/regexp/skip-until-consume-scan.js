// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Coverage for the greedy character-class consume scans emitted by
// ChoiceNode::MaybeEmitFixedLengthConsumeScan ([B]* fused into one
// SkipUntilChar / SkipUntilCharOrChar / SkipUntilCharAnd). The op is not
// observable, so each case just checks the match is unchanged from the generic
// loop. Repeat to encourage tier-up to the native path, where the scan fires.
function check(re, subject, expected) {
  for (let i = 0; i < 3; i++) {
    const m = re.exec(subject);
    const got = m === null ? null : [m.index, ...m];
    assertEquals(expected, got, `${re}.exec(${JSON.stringify(subject)})`);
  }
}

// Single-char exit set -> SkipUntilChar. Atomic continuation (the classic
// /[^"]*"/) and the non-atomic /[^"]*a/ (the stop char is reachable by the
// continuation), which is only correct because the loop's step-back is kept.
check(/[^"]*"/, 'abc"def', [0, 'abc"']);
check(/[^"]*"/, '"', [0, '"']);
check(/[^"]*"/, 'no quote', null);
check(/[^a]*a/, 'xxxa', [0, 'xxxa']);
check(/[^"]*a/, '""xxa', [2, 'xxa']);  // 'a' is in the body class [^"].
check(
    /([^"]*)"(.*)/, 'k"v', [0, 'k"v', 'k', 'v']);  // captures around the loop.

// Two-char exit set -> SkipUntilCharOrChar. '.' is [^\n\r]; its two exit chars
// do not form a masked class.
check(/.*x/, 'aaax', [0, 'aaax']);
check(/.*x/, 'a\nbx', [2, 'bx']);      // '.' stops at '\n'.
check(/.*x/s, 'a\nbx', [0, 'a\nbx']);  // dotAll: '.' matches '\n' too.

// Masked exit set -> SkipUntilCharAnd. Under /i the exit set {a, A} differs
// only in the case bit.
check(/[^a]*a/i, 'XXXA', [0, 'XXXA']);
check(/[^a]*a/i, 'aBc', [0, 'a']);
check(/[^k]*k/i, 'fooKbar', [0, 'fooK']);
check(/[^a]*b/i, 'AAAb', [3, 'b']);  // [^a] excludes both 'a' and 'A'.

// Larger exit sets that are not a single masked class fall back to the generic
// loop; results must still be correct.
check(/[^abc]*c/, 'xyzC', null);  // case-sensitive: no 'c'.
check(/[^abc]*c/, 'xyzc', [0, 'xyzc']);
check(/[a-y]*z/, 'abcz', [0, 'abcz']);

// Boundaries: stop char at the very start, at the very end, and an empty
// subject.
check(/[^a]*a/, 'a', [0, 'a']);
check(/[^a]*/, '', [0, '']);
check(/[^a]*a/, '', null);

// Two-byte subjects (non-unicode): the scan compares single code units, so a
// BMP stop char -- including a two-byte one -- is found correctly. Under /u or
// /v the scan bails to the generic loop (the one-code-unit step-back is unsafe
// across surrogate pairs); results stay correct either way.
check(/[^x]*x/, 'ΩΩx', [0, 'ΩΩx']);    // stop at 'x' past two-byte chars.
check(/[^Ω]*Ω/, 'abΩ', [0, 'abΩ']);  // two-byte stop char.
check(/[^a]*a/iu, 'XX\u{1f0a1}A', [0, 'XX\u{1f0a1}A']);  // /u + surrogate.

// The single-class check only inspects the first node of the iteration, so a
// body that keeps consuming past it -- here '.' then 'z' -- has text_length > 1
// and must not be fused. Regression for a crash / miscompile on such bodies.
check(/(?:.z)+/, 'azbz', [0, 'azbz']);
check(/(?:.z)+/, 'xz yz zz', [0, 'xz']);
check(/(?:.z)+/v, 'azbzcz', [0, 'azbzcz']);
check(/(?:.z)+/v, '', null);
