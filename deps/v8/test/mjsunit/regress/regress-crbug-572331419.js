// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unanchored regexps containing an anchor get the implicit `.*?` search
// prefix unrolled once. In unicode mode the unrolled step must advance by
// code point (AdvanceStringIndex), never landing between the halves of a
// surrogate pair.

// Only matches the empty string; must not match between surrogates.
assertFalse(/(?<![\s\S])(?:(?:^)*)(?![\s\S])/mu.test("\u{1F600}"));
assertTrue(/(?<![\s\S])(?:(?:^)*)(?![\s\S])/mu.test(""));

// Position 1 (inside the pair) is not a valid start in unicode mode.
assertEquals(2, /(?!.)|a^/u.exec("\u{1F600}").index);
assertEquals(2, /\n|(?!^)/u.exec("\u{1F600}").index);
assertEquals(2, /(?!.)|a^/v.exec("\u{1F600}").index);
assertEquals(2, /(?!.)|a^/ui.exec("\u{1F600}").index);
assertEquals(2, /(?!.)|a^/mu.exec("\u{1F600}").index);
// Control: no anchor, no unrolling.
assertEquals(2, /(?!.)/u.exec("\u{1F600}").index);
// Pair not at the unrolled position.
assertEquals(4, /(?!.)|a^/u.exec("a\u{1F600}b").index);

// Lone surrogates are code points of their own.
assertEquals(1, /(?!.)|a^/u.exec("\uD83D").index);
assertEquals(1, /(?!.)|a^/u.exec("\uDE00").index);
assertEquals(2, /(?!.)|a^/u.exec("\uDE00\uD83D").index);

// Without /u, code units are the unit of advance.
assertEquals(1, /\n|(?!^)/.exec("\u{1F600}").index);

// Global / sticky variants take the OptionallyStepBackToLeadSurrogate path.
assertEquals(2, /(?!.)|a^/gu.exec("\u{1F600}").index);
assertEquals(null, /(?!.)|a^/yu.exec("\u{1F600}"));
assertEquals([""], "\u{1F600}".match(/(?!.)|a^/gu));
assertEquals(["\u{1F600}"], "\u{1F600}".split(/\n|(?!^)/u));
