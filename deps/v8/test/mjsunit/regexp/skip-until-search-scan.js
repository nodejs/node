// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Coverage for the unanchored-search prefix scans emitted by
// ChoiceNode::EmitSkipUntilSearchPrelude: the search to the first position
// where the body can match is fused into SkipUntilChar / SkipUntilCharAnd /
// SkipUntilCharOrChar / SkipUntilBitInTable. The op is not observable, so each
// case just checks the match is unchanged from the generic search. Repeat to
// encourage tier-up to the native path, where the scan fires.
function check(re, subject, expected) {
  for (let i = 0; i < 3; i++) {
    const m = re.exec(subject);
    const got = m === null ? null : [m.index, ...m];
    assertEquals(expected, got, `${re}.exec(${JSON.stringify(subject)})`);
  }
}

// Singleton leading class / leading atom / leading sub-loop -> SkipUntilChar.
check(/[a]/, 'zzza', [3, 'a']);
check(/a+/, 'zzzaa', [3, 'aa']);
check(/abc/, 'xxabc', [2, 'abc']);
check(/[a]/, 'a', [0, 'a']);
check(/[a]/, 'zzz', null);

// Case-folded leading class {a, A} -> SkipUntilCharAnd.
check(/[a]/i, 'ZZa', [2, 'a']);
check(/a+/i, 'ZZAa', [2, 'Aa']);

// Two-char leading class that is not a masked pair -> SkipUntilCharOrChar.
check(/[ax]y/, 'zzaay', [3, 'ay']);
check(/[ax]y/, 'xy', [0, 'xy']);
check(/[ax]y/, 'zzz', null);

// Larger leading class ([a-e], >2 chars) has no exact small scan, so the
// prelude falls back to the body's quick check: {a..e} share the high bits, so
// the mask stops on a superset of the class (here [`a-g]). The body re-check
// filters the extra positions, so results match the generic search even when
// the scan stops on a non-class char first.
check(/[a-e]x/, 'zzcx', [2, 'cx']);
check(/[a-e]x/, 'fx', null);        // 'f' passes the mask but is not in [a-e].
check(/[a-e]x/, '`cx', [1, 'cx']);  // '`' is a mask false-positive; scan resumes.
check(/[a-e]x/, 'eex', [1, 'ex']);

// Negated leading class -> SkipUntilBitInTable (one-byte only).
check(/[^\n]x/, '\nax', [1, 'ax']);
check(/[^\n]x/, '\nx', null);

// Empty-matchable body: NO skip may be emitted (the empty branch matches at
// position 0); a skip keyed on a later char would wrongly jump past it.
check(/|z/, 'x', [0, '']);
check(/a*/, 'bba', [0, '']);
check(/(2){0,}/, 'q', [0, '', undefined]);
check(/z|/, 'q', [0, '']);

// Two-byte subjects: the family compares single code units, so a BMP leading
// char is found correctly past leading two-byte chars.
check(/[a]/, 'Ωza', [2, 'a']);
check(/a+/, 'Ωzaa', [2, 'aa']);
check(/[a]/i, 'ΩZA', [2, 'A']);
check(/abc/, 'Ωxabc', [2, 'abc']);
