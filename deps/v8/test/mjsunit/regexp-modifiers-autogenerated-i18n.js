// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --regexp-mode-modifiers

// These regexps are just grepped out of the other tests we already have
// and the syntax changed from out-of-line i flag to inline i flag.

// These tests won't all run on the noi18n build of V8.

assertTrue(/(?i)\u00e5/u.test("\u00c5"));
assertTrue(/(?i)\u00e5/u.test("\u00e5"));
assertTrue(/(?i)\u00c5/u.test("\u00e5"));
assertTrue(/(?i)\u00c5/u.test("\u00c5"));
assertTrue(/(?i)\u212b/u.test("\u212b"));
assertFalse(/(?i)\u00df/u.test("SS"));
assertFalse(/(?i)\u1f8d/u.test("\u1f05\u03b9"));
assertTrue(/(?i)\u1f6b/u.test("\u1f63"));
assertTrue(/(?i)\u00e5/u.test("\u212b"));
assertTrue(/(?i)\u00e5/u.test("\u00c5"));
assertTrue(/(?i)\u00e5/u.test("\u00e5"));
assertTrue(/(?i)\u00e5/u.test("\u212b"));
assertTrue(/(?i)\u00c5/u.test("\u00e5"));
assertTrue(/(?i)\u00c5/u.test("\u212b"));
assertTrue(/(?i)\u00c5/u.test("\u00c5"));
assertTrue(/(?i)\u212b/u.test("\u00c5"));
assertTrue(/(?i)\u212b/u.test("\u00e5"));
assertTrue(/(?i)\u212b/u.test("\u212b"));
assertTrue(/(?i)\u{10400}/u.test("\u{10428}"));
assertTrue(/(?i)\ud801\udc00/u.test("\u{10428}"));
assertTrue(/(?i)[\u{10428}]/u.test("\u{10400}"));
assertTrue(/(?i)[\ud801\udc28]/u.test("\u{10400}"));
assertFalse(/(?i)\u00df/u.test("SS"));
assertFalse(/(?i)\u1f8d/u.test("\u1f05\u03b9"));
assertTrue(/(?i)\u1f8d/u.test("\u1f85"));
assertTrue(/(?i)\u1f6b/u.test("\u1f63"));
assertTrue(/(?i)\u00e5\u00e5\u00e5/u.test("\u212b\u00e5\u00c5"));
assertTrue(/(?i)AB\u{10400}/u.test("ab\u{10428}"));
assertTrue(/(?i)\w/u.test('\u017F'));
assertTrue(/(?i)\w/u.test('\u212A'));
assertFalse(/(?i)\W/u.test('\u017F'));
assertFalse(/(?i)\W/u.test('\u212A'));
assertFalse(/(?i)\W/u.test('s'));
assertFalse(/(?i)\W/u.test('S'));
assertFalse(/(?i)\W/u.test('K'));
assertFalse(/(?i)\W/u.test('k'));
assertTrue(/(?i)[\w]/u.test('\u017F'));
assertTrue(/(?i)[\w]/u.test('\u212A'));
assertFalse(/(?i)[\W]/u.test('\u017F'));
assertFalse(/(?i)[\W]/u.test('\u212A'));
assertFalse(/(?i)[\W]/u.test('s'));
assertFalse(/(?i)[\W]/u.test('S'));
assertFalse(/(?i)[\W]/u.test('K'));
assertFalse(/(?i)[\W]/u.test('k'));
assertTrue(/(?i)\b/u.test('\u017F'));
assertTrue(/(?i)\b/u.test('\u212A'));
assertTrue(/(?i)\b/u.test('s'));
assertTrue(/(?i)\b/u.test('S'));
assertFalse(/(?i)\B/u.test('\u017F'));
assertFalse(/(?i)\B/u.test('\u212A'));
assertFalse(/(?i)\B/u.test('s'));
assertFalse(/(?i)\B/u.test('S'));
assertFalse(/(?i)\B/u.test('K'));
assertFalse(/(?i)\B/u.test('k'));
assertTrue(/(?i)\p{Ll}/u.test("a"));
assertTrue(/(?i)\p{Ll}/u.test("\u{118D4}"));
assertTrue(/(?i)\p{Ll}/u.test("A"));
assertTrue(/(?i)\p{Ll}/u.test("\u{118B4}"));
assertTrue(/(?i)\P{Ll}/u.test("a"));
assertTrue(/(?i)\P{Ll}/u.test("\u{118D4}"));
assertTrue(/(?i)\P{Ll}/u.test("A"));
assertTrue(/(?i)\P{Ll}/u.test("\u{118B4}"));
assertTrue(/(?i)\p{Lu}/u.test("a"));
assertTrue(/(?i)\p{Lu}/u.test("\u{118D4}"));
assertTrue(/(?i)\p{Lu}/u.test("A"));
assertTrue(/(?i)\p{Lu}/u.test("\u{118B4}"));
assertTrue(/(?i)\P{Lu}/u.test("a"));
assertTrue(/(?i)\P{Lu}/u.test("\u{118D4}"));
assertTrue(/(?i)\P{Lu}/u.test("A"));
assertTrue(/(?i)\P{Lu}/u.test("\u{118B4}"));
