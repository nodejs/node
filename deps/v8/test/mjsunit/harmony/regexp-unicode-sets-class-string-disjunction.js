// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Case-equivalence closure for single-character strings in \q{...} with /vi.
assertTrue(/^[\q{a}]$/vi.test('a'));
assertTrue(/^[\q{a}]$/vi.test('A'));
assertTrue(/^[\q{A}]$/vi.test('a'));
assertTrue(/^[\q{A}]$/vi.test('A'));

// Mixed single-character and multi-character strings in \q{...} with /vi.
assertTrue(/^[\q{a|bc}]$/vi.test('a'));
assertTrue(/^[\q{a|bc}]$/vi.test('A'));
assertTrue(/^[\q{a|bc}]$/vi.test('bc'));
assertTrue(/^[\q{a|bc}]$/vi.test('BC'));
assertTrue(/^[\q{a|bc}]$/vi.test('Bc'));
assertTrue(/^[\q{A|BC}]$/vi.test('a'));
assertTrue(/^[\q{A|BC}]$/vi.test('A'));
assertTrue(/^[\q{A|BC}]$/vi.test('bc'));
assertTrue(/^[\q{a|}]$/vi.test('A'));
assertTrue(/^[\q{a|}]$/vi.test(''));

// Set operations (intersection, subtraction, complement) with single-character
// \q{...} and /vi.
assertTrue(/^[\q{a}&&[A]]$/vi.test('A'));
assertTrue(/^[\q{a}&&[A]]$/vi.test('a'));
assertTrue(/^[\q{A}&&[a]]$/vi.test('A'));
assertTrue(/^[\q{A}&&[a]]$/vi.test('a'));
assertTrue(/^[\q{a}&&\q{A}]$/vi.test('A'));
assertTrue(/^[\q{a}&&\q{A}]$/vi.test('a'));
assertTrue(/^[\q{a|bc}&&[A]]$/vi.test('A'));
assertFalse(/^[\q{a|bc}&&[A]]$/vi.test('bc'));

assertFalse(/^[[A]--\q{a}]$/vi.test('A'));
assertFalse(/^[[A]--\q{a}]$/vi.test('a'));
assertFalse(/^[[a]--\q{A}]$/vi.test('A'));
assertFalse(/^[[a]--\q{A}]$/vi.test('a'));
assertFalse(/^[[a-c]--\q{B}]$/vi.test('b'));
assertFalse(/^[[a-c]--\q{B}]$/vi.test('B'));
assertTrue(/^[[a-c]--\q{B}]$/vi.test('A'));
assertTrue(/^[[a-c]--\q{B}]$/vi.test('C'));
assertFalse(/^[\q{a|b}--[A]]$/vi.test('A'));
assertFalse(/^[\q{a|b}--[A]]$/vi.test('a'));
assertTrue(/^[\q{a|b}--[A]]$/vi.test('B'));

assertFalse(/^[^\q{a}]$/vi.test('A'));
assertFalse(/^[^\q{a}]$/vi.test('a'));
assertFalse(/^[^\q{A}]$/vi.test('a'));
assertFalse(/^[^\q{A}]$/vi.test('A'));
assertTrue(/^[^\q{a}]$/vi.test('B'));

// Non-ASCII Unicode simple case folding in single-character \q{...} with /vi.
// U+212A KELVIN SIGN <-> 'k', 'K'
assertTrue(/^[\q{k}]$/vi.test('\u212A'));
assertTrue(/^[\q{K}]$/vi.test('\u212A'));
assertTrue(/^[\q{\u212A}]$/vi.test('k'));
assertTrue(/^[\q{\u212A}]$/vi.test('K'));
assertFalse(/^[[K]--\q{\u212A}]$/vi.test('k'));
assertFalse(/^[[K]--\q{\u212A}]$/vi.test('K'));
assertFalse(/^[[K]--\q{\u212A}]$/vi.test('\u212A'));

// U+017F LATIN SMALL LETTER LONG S <-> 's', 'S'
assertTrue(/^[\q{\u017F}]$/vi.test('s'));
assertTrue(/^[\q{\u017F}]$/vi.test('S'));
assertTrue(/^[\q{s}]$/vi.test('\u017F'));
assertTrue(/^[\q{S}]$/vi.test('\u017F'));

// U+00B5 MICRO SIGN <-> U+03BC GREEK SMALL LETTER MU, U+039C GREEK CAPITAL LETTER MU
assertTrue(/^[\q{\u00B5}]$/vi.test('\u03BC'));
assertTrue(/^[\q{\u00B5}]$/vi.test('\u039C'));

// Non-BMP (supplementary) case folding: Deseret U+10400 <-> U+10428
assertTrue(/^[\q{\u{10400}}]$/vi.test('\u{10428}'));
assertTrue(/^[\q{\u{10428}}]$/vi.test('\u{10400}'));
assertFalse(/^[\q{\u{10400}}--\q{\u{10428}}]$/vi.test('\u{10400}'));
assertFalse(/^[\q{\u{10400}}--\q{\u{10428}}]$/vi.test('\u{10428}'));

// Isolated escaped surrogates in \q{...} must not combine into a single astral
// code point.
assertFalse(/^[\q{\u{D800}\u{DC00}}]$/v.test('\u{10000}'));
assertFalse(/^[\q{\uD800\u{DC00}}]$/v.test('\u{10000}'));
assertFalse(/^[\q{\u{D800}\uDC00}]$/v.test('\u{10000}'));
assertFalse(/^[\q{\u{D800}\u{DC00}}]$/vi.test('\u{10000}'));

assertThrows('/^[^\\q{\\u{D800}\\u{DC00}}]$/v', SyntaxError);
assertThrows('/^[^\\q{\\uD800\\u{DC00}}]$/v', SyntaxError);
assertThrows('/^[^\\q{\\u{D800}\\uDC00}]$/v', SyntaxError);

// 4-hex-digit surrogate pairs and single astral code point escapes in \q{...}
// are single code points.
assertTrue(/^[\q{\uD800\uDC00}]$/v.test('\u{10000}'));
assertTrue(/^[\q{\u{10000}}]$/v.test('\u{10000}'));
assertFalse(/^[^\q{\uD800\uDC00}]$/v.test('\u{10000}'));
assertFalse(/^[^\q{\u{10000}}]$/v.test('\u{10000}'));
