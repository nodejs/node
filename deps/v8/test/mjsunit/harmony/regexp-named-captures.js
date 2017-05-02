// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-regexp-named-captures

// Malformed named captures.
assertThrows("/(?<>a)/u");  // Empty name.
assertThrows("/(?<aa)/u");  // Unterminated name.
assertThrows("/(?<42a>a)/u");  // Name starting with digits.
assertThrows("/(?<:a>a)/u");  // Name starting with invalid char.
assertThrows("/(?<a:>a)/u");  // Name containing with invalid char.
assertThrows("/(?<a>a)(?<a>a)/u");  // Duplicate name.
assertThrows("/(?<a>a)(?<b>b)(?<a>a)/u");  // Duplicate name.
assertThrows("/\\k<a>/u");  // Invalid reference.
assertThrows("/(?<a>a)\\k<ab>/u");  // Invalid reference.
assertThrows("/(?<ab>a)\\k<a>/u");  // Invalid reference.
assertThrows("/\\k<a>(?<ab>a)/u");  // Invalid reference.

// Fallback behavior in non-unicode mode.
assertThrows("/(?<>a)/", SyntaxError);
assertThrows("/(?<aa)/", SyntaxError);
assertThrows("/(?<42a>a)/", SyntaxError);
assertThrows("/(?<:a>a)/", SyntaxError);
assertThrows("/(?<a:>a)/", SyntaxError);
assertThrows("/(?<a>a)(?<a>a)/", SyntaxError);
assertThrows("/(?<a>a)(?<b>b)(?<a>a)/", SyntaxError);
assertThrows("/(?<a>a)\\k<ab>/", SyntaxError);
assertThrows("/(?<ab>a)\\k<a>/", SyntaxError);

assertEquals(["k<a>"], "xxxk<a>xxx".match(/\k<a>/));
assertEquals(["k<a"], "xxxk<a>xxx".match(/\k<a/));

// Basic named groups.
assertEquals(["a", "a"], "bab".match(/(?<a>a)/u));
assertEquals(["a", "a"], "bab".match(/(?<a42>a)/u));
assertEquals(["a", "a"], "bab".match(/(?<_>a)/u));
assertEquals(["a", "a"], "bab".match(/(?<$>a)/u));
assertEquals(["bab", "a"], "bab".match(/.(?<$>a)./u));
assertEquals(["bab", "a", "b"], "bab".match(/.(?<a>a)(.)/u));
assertEquals(["bab", "a", "b"], "bab".match(/.(?<a>a)(?<b>.)/u));
assertEquals(["bab", "ab"], "bab".match(/.(?<a>\w\w)/u));
assertEquals(["bab", "bab"], "bab".match(/(?<a>\w\w\w)/u));
assertEquals(["bab", "ba", "b"], "bab".match(/(?<a>\w\w)(?<b>\w)/u));

assertEquals("bab".match(/(a)/u), "bab".match(/(?<a>a)/u));
assertEquals("bab".match(/(a)/u), "bab".match(/(?<a42>a)/u));
assertEquals("bab".match(/(a)/u), "bab".match(/(?<_>a)/u));
assertEquals("bab".match(/(a)/u), "bab".match(/(?<$>a)/u));
assertEquals("bab".match(/.(a)./u), "bab".match(/.(?<$>a)./u));
assertEquals("bab".match(/.(a)(.)/u), "bab".match(/.(?<a>a)(.)/u));
assertEquals("bab".match(/.(a)(.)/u), "bab".match(/.(?<a>a)(?<b>.)/u));
assertEquals("bab".match(/.(\w\w)/u), "bab".match(/.(?<a>\w\w)/u));
assertEquals("bab".match(/(\w\w\w)/u), "bab".match(/(?<a>\w\w\w)/u));
assertEquals("bab".match(/(\w\w)(\w)/u), "bab".match(/(?<a>\w\w)(?<b>\w)/u));

assertEquals(["bab", "b"], "bab".match(/(?<b>b).\1/u));
assertEquals(["baba", "b", "a"], "baba".match(/(.)(?<a>a)\1\2/u));
assertEquals(["baba", "b", "a", "b", "a"],
    "baba".match(/(.)(?<a>a)(?<b>\1)(\2)/u));
assertEquals(["<a", "<"], "<a".match(/(?<lt><)a/u));
assertEquals([">a", ">"], ">a".match(/(?<gt>>)a/u));

// Named references.
assertEquals(["bab", "b"], "bab".match(/(?<b>.).\k<b>/u));
assertNull("baa".match(/(?<b>.).\k<b>/u));

// Nested groups.
assertEquals(["bab", "bab", "ab", "b"], "bab".match(/(?<a>.(?<b>.(?<c>.)))/u));

// Reference inside group.
assertEquals(["bab", "b"], "bab".match(/(?<a>\k<a>\w)../u));

// Reference before group.
assertEquals(["bab", "b"], "bab".match(/\k<a>(?<a>b)\w\k<a>/u));
assertEquals(["bab", "b", "a"], "bab".match(/(?<b>b)\k<a>(?<a>a)\k<b>/u));

// Reference properties.
assertEquals("a", /(?<a>a)(?<b>b)\k<a>/u.exec("aba").group.a);
assertEquals("b", /(?<a>a)(?<b>b)\k<a>/u.exec("aba").group.b);
assertEquals(undefined, /(?<a>a)(?<b>b)\k<a>/u.exec("aba").group.c);
assertEquals(undefined, /(?<a>a)(?<b>b)\k<a>|(?<c>c)/u.exec("aba").group.c);

// Unicode names.
assertEquals("a", /(?<π>a)/u.exec("bab").group.π);
assertEquals("a", /(?<\u{03C0}>a)/u.exec("bab").group.\u03C0);
assertEquals("a", /(?<$>a)/u.exec("bab").group.$);
assertEquals("a", /(?<_>a)/u.exec("bab").group._);
assertEquals("a", /(?<$𐒤>a)/u.exec("bab").group.$𐒤);
assertEquals("a", /(?<_\u200C>a)/u.exec("bab").group._\u200C);
assertEquals("a", /(?<_\u200D>a)/u.exec("bab").group._\u200D);
assertEquals("a", /(?<ಠ_ಠ>a)/u.exec("bab").group.ಠ_ಠ);
assertThrows('/(?<❤>a)/u', SyntaxError);
assertThrows('/(?<𐒤>a)/u', SyntaxError);  // ID_Continue but not ID_Start.

// The '__proto__' property on the groups object.
assertEquals(undefined, /(?<a>.)/u.exec("a").group.__proto__);
assertEquals("a", /(?<__proto__>a)/u.exec("a").group.__proto__);
