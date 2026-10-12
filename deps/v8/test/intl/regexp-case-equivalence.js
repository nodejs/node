// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Exercise literals, ranges, and backreferences with the same case pairs.
const pairs = [
  ['a', 'A', true, true],
  ['k', '\u212a', false, true],
  ['s', '\u017f', false, true],
  ['\u00df', '\u1e9e', false, true],
  ['\u00b5', '\u03bc', true, true],
  ['i', '\u0130', false, false],
  ['I', '\u0131', false, false],
  ['\u{10400}', '\u{10428}', false, true],
  ['\ud800', '\ud800', true, true],
  ['\ud800', '\ud801', false, false],
];

for (const [a, b, legacy, unicode] of pairs) {
  for (const [c1, c2] of [[a, b], [b, a]]) {
    for (const flags of ['i', 'iu', 'iv']) {
      const expected = flags === 'i' ? legacy : unicode;
      // Force two-byte subjects even when the pair is entirely Latin1.
      for (const prefix of ['', '\u2603']) {
        const literal = new RegExp('^' + prefix + c1 + '$', flags);
        const backref = new RegExp('^' + prefix + '(' + c1 + ')\\1$', flags);
        // A legacy character class cannot represent an astral character.
        const range = flags !== 'i' || c1.length === 1
            ? new RegExp('^' + prefix + '[' + c1 + '-' + c1 + ']$', flags)
            : null;
        assertEquals(expected, literal.test(prefix + c2), literal.toString());
        assertEquals(expected, backref.test(prefix + c1 + c2),
                     backref.toString());
        if (range !== null) {
          assertEquals(expected, range.test(prefix + c2), range.toString());
        }
      }
    }
  }
}

for (const flags of ['i', 'iu', 'iv']) {
  const re = new RegExp('^[a-c\u017f\u00df]$', flags);
  for (const c of ['a', 'B', 'c', '\u017f', '\u00df']) {
    assertTrue(re.test(c));
  }
  for (const c of ['s', 'S', '\u1e9e']) {
    assertEquals(flags !== 'i', re.test(c));
  }
}

// Sorting must preserve the preference between equivalent alternatives.
for (const flags of ['i', 'iu', 'iv']) {
  assertEquals('is', new RegExp('is|I|ix|z', flags).exec('is')[0]);
}
