// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Factoring /i alternatives by common prefix must not equate U+017F and 's'.

assertTrue(/^(?:\u017fa|sb|\u017fc|sd)$/i.test('sb'));
assertFalse(/^(?:\u017fa|sb|\u017fc|sd)$/i.test('\u017fb'));
assertTrue(/^(?:\u212aa|kb|\u212ac|kd)$/i.test('Kb'));
assertFalse(/^(?:\u212aa|kb|\u212ac|kd)$/i.test('\u212ab'));
assertTrue(/^(?:\u017fa|sb|\u017fc|sd)$/iu.test('\u017fb'));

// A disjunction must match exactly what its alternatives match individually.
function TestAlternatives(c1, c2, flags) {
  for (const prefix of ['', 'q']) {
    const alternatives =
        [c1 + 'a', c2 + 'b', c1 + 'c', c2 + 'd'].map(a => prefix + a);
    const re = new RegExp('^(?:' + alternatives.join('|') + ')$', flags);
    const singles = alternatives.map(a => new RegExp('^' + a + '$', flags));
    for (const c of [c1, c2]) {
      for (const suffix of 'abcd') {
        const s = prefix + c + suffix;
        assertEquals(singles.some(r => r.test(s)), re.test(s), re + ' ' + s);
      }
    }
  }
}

// Interesting case-folding equivalence classes, see intl/regress-10248.js.
const equivalence_classes = [
  '\u0041\u0061',              // Aa (sanity check)
  '\u004b\u006b\u212a',        // KkK
  '\u0053\u0073\u017f',        // Ssſ
  '\u00b5\u039c\u03bc',        // µΜμ
  '\u00c5\u00e5\u212b',        // ÅåÅ
  '\u00df\u1e9e',              // ßẞ
  '\u03a9\u03c9\u2126',        // ΩωΩ
  '\u0390\u1fd3',              // ΐΐ
  '\u0398\u03b8\u03d1\u03f4',  // Θθϑϴ
  '\u03b0\u1fe3',              // ΰΰ
  '\u1f80\u1f88',              // ᾀᾈ
  '\u1fb3\u1fbc',              // ᾳᾼ
  '\u1fc3\u1fcc',              // ῃῌ
  '\u1ff3\u1ffc',              // ῳῼ
  '\ufb05\ufb06',              // ﬅﬆ
];

for (const eclass of equivalence_classes) {
  for (const c1 of eclass) {
    for (const c2 of eclass) {
      if (c1 === c2) continue;
      for (const flags of ['i', 'iu', 'iv']) {
        TestAlternatives(c1, c2, flags);
      }
    }
  }
}
