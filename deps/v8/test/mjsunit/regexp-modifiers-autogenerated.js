// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --regexp-mode-modifiers --harmony-regexp-property

// These regexps are just grepped out of the other tests we already have
// and the syntax changed from out-of-line i flag to inline i flag.

assertFalse(/(?i)x(...)\1/.test("x\u03a3\u03c2\u03c3\u03c2\u03c3"));
assertTrue(/(?i)\u03a3((?:))\1\1x/.test("\u03c2x"), "backref-UC16-empty");
assertTrue(/(?i)x(?:...|(...))\1x/.test("x\u03a3\u03c2\u03c3x"));
assertTrue(/(?i)x(?:...|(...))\1x/.test("x\u03c2\u03c3\u039b\u03a3\u03c2\u03bbx"));
assertFalse(/(?i)\xc1/.test('fooA'), "quickcheck-uc16-pattern-ascii-subject");
assertFalse(/(?i)x(...)\1/.test("xaaaaa"), "backref-ASCII-short");
assertTrue(/(?i)x((?:))\1\1x/.test("xx"), "backref-ASCII-empty");
assertTrue(/(?i)x(?:...|(...))\1x/.test("xabcx"), "backref-ASCII-uncaptured");
assertTrue(/(?i)x(?:...|(...))\1x/.test("xabcABCx"), "backref-ASCII-backtrack");
assertFalse(/(?i)f/.test('b'));
assertFalse(/(?i)[abc]f/.test('x'));
assertFalse(/(?i)[abc]f/.test('xa'));
assertFalse(/(?i)[abc]</.test('x'));
assertFalse(/(?i)[abc]</.test('xa'));
assertFalse(/(?i)f[abc]/.test('x'));
assertFalse(/(?i)f[abc]/.test('xa'));
assertFalse(/(?i)<[abc]/.test('x'));
assertFalse(/(?i)<[abc]/.test('xa'));
assertFalse(/(?i)[\u00e5]/.test("\u212b"));
assertFalse(/(?i)[\u212b]/.test("\u00e5\u1234"));
assertFalse(/(?i)[\u212b]/.test("\u00e5"));
assertFalse(/(?i)\u{10400}/.test("\u{10428}"));
assertFalse(/(?i)[\u00e5]/.test("\u212b"));
assertFalse(/(?i)[\u212b]/.test("\u00e5\u1234"));
assertFalse(/(?i)[\u212b]/.test("\u00e5"));
assertFalse(/(?i)\u{10400}/.test("\u{10428}"));
assertTrue(/(?i)[@-A]/.test("a"));
assertTrue(/(?i)[@-A]/.test("A"));
assertTrue(/(?i)[@-A]/.test("@"));
assertFalse(/(?i)[¿-À]/.test('¾'));
assertTrue(/(?i)[¿-À]/.test('¿'));
assertTrue(/(?i)[¿-À]/.test('À'));
assertTrue(/(?i)[¿-À]/.test('à'));
assertFalse(/(?i)[¿-À]/.test('á'));
assertFalse(/(?i)[¿-À]/.test('Á'));
assertFalse(/(?i)[¿-À]/.test('Á'));
assertFalse(/(?i)[Ö-×]/.test('Õ'));
assertTrue(/(?i)[Ö-×]/.test('Ö'));
assertTrue(/(?i)[Ö-×]/.test('ö'));
assertTrue(/(?i)[Ö-×]/.test('×'));
assertFalse(/(?i)[Ö-×]/.test('Ø'));
assertTrue(/(?i)(a[\u1000A])+/.test('aa'));
assertTrue(/(?i)\u0178/.test('\u00ff'));
assertTrue(/(?i)\u039c/.test('\u00b5'));
assertTrue(/(?i)\u039c/.test('\u03bc'));
assertTrue(/(?i)\u00b5/.test('\u03bc'));
assertTrue(/(?i)[\u039b-\u039d]/.test('\u00b5'));
assertFalse(/(?i)[^\u039b-\u039d]/.test('\u00b5'));

assertTrue(/(?m)^bar/.test("bar"));
assertTrue(/(?m)^bar/.test("bar\nfoo"));
assertTrue(/(?m)^bar/.test("foo\nbar"));
assertTrue(/(?m)bar$/.test("bar"));
assertTrue(/(?m)bar$/.test("bar\nfoo"));
assertTrue(/(?m)bar$/.test("foo\nbar"));
assertFalse(/(?m)^bxr/.test("bar"));
assertFalse(/(?m)^bxr/.test("bar\nfoo"));
assertFalse(/(?m)^bxr/.test("foo\nbar"));
assertFalse(/(?m)bxr$/.test("bar"));
assertFalse(/(?m)bxr$/.test("bar\nfoo"));
assertFalse(/(?m)bxr$/.test("foo\nbar"));
assertTrue(/(?m)^.*$/.test("\n"));
assertTrue(/(?m)^([()]|.)*$/.test("()\n()"));
assertTrue(/(?m)^([()]|.)*$/.test("()\n"));
assertTrue(/(?m)^[()]*$/.test("()\n."));
