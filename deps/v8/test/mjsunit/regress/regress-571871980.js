// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Character classes in one-byte pattern sources were marked as certainly
// matching one code point, even in unicode mode where \u{...}, \uXXXX\uXXXX
// and \p{...} can describe supplementary code points that match a surrogate
// pair. The resulting too-small max_match broke the end-anchor optimization.

const tests = [
  // One-byte pattern, end-anchored, class containing a surrogate pair.
  { re: /x[\u{1F600}]$/u, str: 'x\u{1F600}', expected: true },
  { re: /x[\uD83D\uDE00]$/u, str: 'x\u{1F600}', expected: true },
  { re: /x[\p{Emoji_Presentation}]$/u, str: 'x\u{1F600}', expected: true },
  { re: /[\u{1F600}][\u{1F600}]$/u, str: '\u{1F600}\u{1F600}', expected: true },
  { re: /x[\u{1F600}]$/iu, str: 'x\u{1F600}', expected: true },
  // Negated classes match surrogate pairs in unicode mode as well.
  { re: /x[^a]$/u, str: 'x\u{1F600}', expected: true },
  { re: /x[^a]$/iu, str: 'x\u{1F600}', expected: true },
  // Two-byte pattern sources.
  { re: /\u{10000}[\u{1F600}]$/u, str: '\u{10000}\u{1F600}', expected: true },
  { re: /\u{10000}[^a]$/u, str: '\u{10000}\u{1F600}', expected: true },

  // Controls: no end anchor, start anchor, /v.
  { re: /x[\u{1F600}]/u, str: 'x\u{1F600}', expected: true },
  { re: /^x[\u{1F600}]$/u, str: 'x\u{1F600}', expected: true },
  { re: /x[\u{1F600}]$/v, str: 'x\u{1F600}', expected: true },

  // Non-unicode mode: classes match a single code unit.
  { re: /x[\uD83D\uDE00]$/, str: 'x\u{1F600}', expected: false },
  { re: /x[\uD83D\uDE00][\uD83D\uDE00]$/, str: 'x\u{1F600}', expected: true },
  { re: /x[\uD83D]$/, str: 'x\u{1F600}', expected: false },
  { re: /x[^a]$/, str: 'x\u{1F600}', expected: false },
  { re: /x[^a][^a]$/, str: 'x\u{1F600}', expected: true },
];

for (const { re, str, expected } of tests) {
  assertEquals(expected, re.test(str), re.toString());
  const match = re.exec(str);
  if (expected) {
    assertNotNull(match, re.toString());
  } else {
    assertNull(match, re.toString());
  }
}
