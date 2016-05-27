// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-regexp-property --harmony-unicode-regexps

assertThrows("/\\p/u");
assertThrows("/\\p{garbage}/u");
assertThrows("/\\p{}/u");
assertThrows("/\\p{/u");
assertThrows("/\\p}/u");
assertThrows("/\p{Math}/u");
assertThrows("/\p{Bidi_M}/u");
assertThrows("/\p{Hex}/u");

assertTrue(/\p{Ll}/u.test("a"));
assertFalse(/\P{Ll}/u.test("a"));
assertTrue(/\P{Ll}/u.test("A"));
assertFalse(/\p{Ll}/u.test("A"));
assertTrue(/\p{Ll}/u.test("\u{1D7BE}"));
assertFalse(/\P{Ll}/u.test("\u{1D7BE}"));
assertFalse(/\p{Ll}/u.test("\u{1D5E3}"));
assertTrue(/\P{Ll}/u.test("\u{1D5E3}"));

assertTrue(/\p{Ll}/iu.test("a"));
assertTrue(/\p{Ll}/iu.test("\u{118D4}"));
assertTrue(/\p{Ll}/iu.test("A"));
assertTrue(/\p{Ll}/iu.test("\u{118B4}"));
assertFalse(/\P{Ll}/iu.test("a"));
assertFalse(/\P{Ll}/iu.test("\u{118D4}"));
assertFalse(/\P{Ll}/iu.test("A"));
assertFalse(/\P{Ll}/iu.test("\u{118B4}"));

assertTrue(/\p{Lu}/u.test("A"));
assertFalse(/\P{Lu}/u.test("A"));
assertTrue(/\P{Lu}/u.test("a"));
assertFalse(/\p{Lu}/u.test("a"));
assertTrue(/\p{Lu}/u.test("\u{1D5E3}"));
assertFalse(/\P{Lu}/u.test("\u{1D5E3}"));
assertFalse(/\p{Lu}/u.test("\u{1D7BE}"));
assertTrue(/\P{Lu}/u.test("\u{1D7BE}"));

assertTrue(/\p{Lu}/iu.test("a"));
assertTrue(/\p{Lu}/iu.test("\u{118D4}"));
assertTrue(/\p{Lu}/iu.test("A"));
assertTrue(/\p{Lu}/iu.test("\u{118B4}"));
assertFalse(/\P{Lu}/iu.test("a"));
assertFalse(/\P{Lu}/iu.test("\u{118D4}"));
assertFalse(/\P{Lu}/iu.test("A"));
assertFalse(/\P{Lu}/iu.test("\u{118B4}"));

assertTrue(/\p{Sm}/u.test("+"));
assertFalse(/\P{Sm}/u.test("+"));
assertTrue(/\p{Sm}/u.test("\u{1D6C1}"));
assertFalse(/\P{Sm}/u.test("\u{1D6C1}"));

assertTrue(/\pL/u.test("a"));
assertFalse(/\PL/u.test("a"));
assertFalse(/\pL/u.test("1"));
assertTrue(/\PL/u.test("1"));
assertTrue(/\pL/u.test("\u1FAB"));
assertFalse(/\PL/u.test("\u1FAB"));
assertFalse(/\p{L}/u.test("\uA6EE"));
assertTrue(/\P{L}/u.test("\uA6EE"));

assertTrue(/\p{Lowercase_Letter}/u.test("a"));
assertTrue(/\p{Math_Symbol}/u.test("+"));
