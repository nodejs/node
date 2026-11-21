// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unicode 18 assigns U+1DF95 LATIN SMALL LIGATURE LONG S WITH DESCENDER S,
// the first supplementary character that case-folds to a BMP character
// (U+00DF). It is thus case-equivalent to U+00DF and U+1E9E under /iu and
// /iv, but never under /i, which compares UTF-16 code units.
const hasUnicode18 = '\u{1DF95}'.toUpperCase() === 'SS';

// Exercise literals, ranges, backreferences, lookbehinds, and class strings
// with the same case pairs.
const pairs = [
  ['a', 'A', true, true],
  ['k', '\u212a', false, true],
  ['s', '\u017f', false, true],
  ['\u00df', '\u1e9e', false, true],
  ['\u00b5', '\u03bc', true, true],
  ['i', '\u0130', false, false],
  ['I', '\u0131', false, false],
  ['\u{10400}', '\u{10428}', false, true],
  ['\u00df', '\u{1DF95}', false, hasUnicode18],
  ['\u1e9e', '\u{1DF95}', false, hasUnicode18],
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
        // TODO(crbug.com/571793821): Case-insensitive backreferences do not
        // yet match captures of a different UTF-16 length (U+00DF vs U+1DF95).
        if (c1.length === c2.length) {
          assertEquals(expected, backref.test(prefix + c1 + c2),
                       backref.toString());
        }
        if (range !== null) {
          assertEquals(expected, range.test(prefix + c2), range.toString());
        }
        const lookbehind = new RegExp('(?<=^' + prefix + c1 + ')x$', flags);
        assertEquals(expected, lookbehind.test(prefix + c2 + 'x'),
                     lookbehind.toString());
        if (flags === 'iv') {
          const classString = new RegExp(
              '^' + prefix + '[\\q{' + c1 + 'x}]$', flags);
          assertEquals(expected, classString.test(prefix + c2 + 'x'),
                       classString.toString());
        }
      }
    }
  }
}

// Supplementary case equivalents of BMP characters must be matched in
// unicode mode both as plain pattern characters and inside character
// classes, but never in legacy mode.
if (hasUnicode18) {
  const S = '\u{1DF95}';
  const cases = [
    // [pattern, subject, expected]
    ['^\u00df$', S, true],
    ['^\u1e9e$', S, true],
    ['^' + S + '$', '\u00df', true],
    ['^' + S + '$', '\u1e9e', true],
    ['^\\u00df$', S, true],
    ['^\\u{df}$', S, true],
    ['^a\u00dfb$', 'A' + S + 'B', true],
    ['^(?:\u00df)+$', S + '\u00df\u1e9e' + S, true],
    ['^\u00df{2}$', S + '\u1e9e', true],
    ['^\u00df{2}$', S, false],
    ['^(?:x|\u00df)$', S, true],
    ['^[\u00df]$', S, true],
    ['^[^\u00df]$', S, false],
    ['^(\u00df)$', S, true],
  ];
  for (const [pattern, subject, expected] of cases) {
    for (const flags of ['iu', 'iv']) {
      const re = new RegExp(pattern, flags);
      assertEquals(expected, re.test(subject), re.toString() + ' ' + subject);
    }
    // In legacy mode, the astral character consists of two code units,
    // neither of which has case equivalents.
    const re = new RegExp(pattern, 'i');
    assertFalse(re.test(subject), re.toString() + ' ' + subject);
  }
  assertEquals([S, S], /(\u00df)/iu.exec(S));
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
