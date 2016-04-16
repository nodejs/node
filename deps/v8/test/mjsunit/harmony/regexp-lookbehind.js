// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-regexp-lookbehind

// Simple fixed-length matches.
assertEquals(["a"], "a".match(/^.(?<=a)/));
assertNull("b".match(/^.(?<=a)/));
assertEquals(["foo"], "foo1".match(/^f..(?<=.oo)/));
assertEquals(["foo"], "foo2".match(/^f\w\w(?<=\woo)/));
assertNull("boo".match(/^f\w\w(?<=\woo)/));
assertNull("fao".match(/^f\w\w(?<=\woo)/));
assertNull("foa".match(/^f\w\w(?<=\woo)/));
assertEquals(["def"], "abcdef".match(/(?<=abc)\w\w\w/));
assertEquals(["def"], "abcdef".match(/(?<=a.c)\w\w\w/));
assertEquals(["def"], "abcdef".match(/(?<=a\wc)\w\w\w/));
assertEquals(["cde"], "abcdef".match(/(?<=a[a-z])\w\w\w/));
assertEquals(["def"], "abcdef".match(/(?<=a[a-z][a-z])\w\w\w/));
assertEquals(["def"], "abcdef".match(/(?<=a[a-z]{2})\w\w\w/));
assertEquals(["bcd"], "abcdef".match(/(?<=a{1})\w\w\w/));
assertEquals(["cde"], "abcdef".match(/(?<=a{1}b{1})\w\w\w/));
assertEquals(["def"], "abcdef".match(/(?<=a{1}[a-z]{2})\w\w\w/));

// Variable-length matches.
assertEquals(["def"], "abcdef".match(/(?<=[a|b|c]*)[^a|b|c]{3}/));
assertEquals(["def"], "abcdef".match(/(?<=\w*)[^a|b|c]{3}/));

// Start of line matches.
assertEquals(["def"], "abcdef".match(/(?<=^abc)def/));
assertEquals(["def"], "abcdef".match(/(?<=^[a-c]{3})def/));
assertEquals(["def"], "xyz\nabcdef".match(/(?<=^[a-c]{3})def/m));
assertEquals(["ab", "cd", "efg"], "ab\ncd\nefg".match(/(?<=^)\w+/gm));
assertEquals(["ab", "cd", "efg"], "ab\ncd\nefg".match(/\w+(?<=$)/gm));
assertEquals(["ab", "cd", "efg"], "ab\ncd\nefg".match(/(?<=^)\w+(?<=$)/gm));
assertNull("abcdef".match(/(?<=^[^a-c]{3})def/));
assertNull("foooo".match(/"^foooo(?<=^o+)$/));
assertNull("foooo".match(/"^foooo(?<=^o*)$/));
assertEquals(["foo"], "foo".match(/^foo(?<=^fo+)$/));
assertEquals(["foooo"], "foooo".match(/^foooo(?<=^fo*)/));
assertEquals(["foo", "f"], "foo".match(/^(f)oo(?<=^\1o+)$/));
assertEquals(["foo", "f"], "foo".match(/^(f)oo(?<=^\1o+)$/i));
assertEquals(["foo\u1234", "f"], "foo\u1234".match(/^(f)oo(?<=^\1o+).$/i));
assertEquals(["def"], "abcdefdef".match(/(?<=^\w+)def/));
assertEquals(["def", "def"], "abcdefdef".match(/(?<=^\w+)def/g));

// Word boundary matches.
assertEquals(["def"], "abc def".match(/(?<=\b)[d-f]{3}/));
assertEquals(["def"], "ab cdef".match(/(?<=\B)\w{3}/));
assertEquals(["def"], "ab cdef".match(/(?<=\B)(?<=c(?<=\w))\w{3}/));
assertNull("abcdef".match(/(?<=\b)[d-f]{3}/));

// Negative lookbehind.
assertEquals(["abc"], "abcdef".match(/(?<!abc)\w\w\w/));
assertEquals(["abc"], "abcdef".match(/(?<!a.c)\w\w\w/));
assertEquals(["abc"], "abcdef".match(/(?<!a\wc)\w\w\w/));
assertEquals(["abc"], "abcdef".match(/(?<!a[a-z])\w\w\w/));
assertEquals(["abc"], "abcdef".match(/(?<!a[a-z]{2})\w\w\w/));
assertNull("abcdef".match(/(?<!abc)def/));
assertNull("abcdef".match(/(?<!a.c)def/));
assertNull("abcdef".match(/(?<!a\wc)def/));
assertNull("abcdef".match(/(?<!a[a-z][a-z])def/));
assertNull("abcdef".match(/(?<!a[a-z]{2})def/));
assertNull("abcdef".match(/(?<!a{1}b{1})cde/));
assertNull("abcdef".match(/(?<!a{1}[a-z]{2})def/));

// Capturing matches.
assertEquals(["def", "c"], "abcdef".match(/(?<=(c))def/));
assertEquals(["def", "bc"], "abcdef".match(/(?<=(\w{2}))def/));
assertEquals(["def", "bc", "c"], "abcdef".match(/(?<=(\w(\w)))def/));
assertEquals(["def", "a"], "abcdef".match(/(?<=(\w){3})def/));
assertEquals(["d", "bc", undefined], "abcdef".match(/(?<=(bc)|(cd))./));
assertEquals(["c", "a", undefined],
             "abcdef".match(/(?<=([ab]{1,2})\D|(abc))\w/));
assertEquals(["ab", "a", "b"], "abcdef".match(/\D(?<=([ab]+))(\w)/));
assertEquals(["c", "d"], "abcdef".match(/(?<=b|c)\w/g));
assertEquals(["cd", "ef"], "abcdef".match(/(?<=[b-e])\w{2}/g));

// Captures inside negative lookbehind. (They never capture.)
assertEquals(["de", undefined], "abcdef".match(/(?<!(^|[ab]))\w{2}/));

// Nested lookaround.
assertEquals(["ef"], "abcdef".match(/(?<=ab(?=c)\wd)\w\w/));
assertEquals(["ef", "bc"], "abcdef".match(/(?<=a(?=([^a]{2})d)\w{3})\w\w/));
assertEquals(["ef", "bc"],
             "abcdef".match(/(?<=a(?=([bc]{2}(?<!a{2}))d)\w{3})\w\w/));
assertNull("abcdef".match(/(?<=a(?=([bc]{2}(?<!a*))d)\w{3})\w\w/));
assertEquals(["faaa"], "faaao".match(/^faaao?(?<=^f[oa]+(?=o))/));

// Back references.
assertEquals(["b", "b", "bb"], "abb".match(/(.)(?<=(\1\1))/));
assertEquals(["B", "B", "bB"], "abB".match(/(.)(?<=(\1\1))/i));
assertEquals(["aB", "aB", "a"], "aabAaBa".match(/((\w)\w)(?<=\1\2\1)/i));
assertEquals(["Ba", "Ba", "a"], "aabAaBa".match(/(\w(\w))(?<=\1\2\1)/i));
assertEquals(["b", "b", "B"], "abaBbAa".match(/(?=(\w))(?<=(\1))./i));
assertEquals(["foo", "'", "foo"], "  'foo'  ".match(/(?<=(.))(\w+)(?=\1)/));
assertEquals(["foo", "\"", "foo"], "  \"foo\"  ".match(/(?<=(.))(\w+)(?=\1)/));
assertNull("  .foo\"  ".match(/(?<=(.))(\w+)(?=\1)/));
assertNull("ab".match(/(.)(?<=\1\1\1)/));
assertNull("abb".match(/(.)(?<=\1\1\1)/));
assertEquals(["b", "b"], "abbb".match(/(.)(?<=\1\1\1)/));
assertNull("ab".match(/(..)(?<=\1\1\1)/));
assertNull("abb".match(/(..)(?<=\1\1\1)/));
assertNull("aabb".match(/(..)(?<=\1\1\1)/));
assertNull("abab".match(/(..)(?<=\1\1\1)/));
assertNull("fabxbab".match(/(..)(?<=\1\1\1)/));
assertNull("faxabab".match(/(..)(?<=\1\1\1)/));
assertEquals(["ab", "ab"], "fababab".match(/(..)(?<=\1\1\1)/));

// Back references to captures inside the lookbehind.
assertEquals(["d", "C"], "abcCd".match(/(?<=\1(\w))d/i));
assertEquals(["d", "x"], "abxxd".match(/(?<=\1([abx]))d/));
assertEquals(["c", "ab"], "ababc".match(/(?<=\1(\w+))c/));
assertEquals(["c", "b"], "ababbc".match(/(?<=\1(\w+))c/));
assertNull("ababdc".match(/(?<=\1(\w+))c/));
assertEquals(["c", "abab"], "ababc".match(/(?<=(\w+)\1)c/));

// Alternations are tried left to right,
// and we do not backtrack into a lookbehind.
assertEquals(["xabcd", "cd", ""], "xabcd".match(/.*(?<=(..|...|....))(.*)/));
assertEquals(["xabcd", "bcd", ""], "xabcd".match(/.*(?<=(xx|...|....))(.*)/));
assertEquals(["xxabcd", "bcd", ""], "xxabcd".match(/.*(?<=(xx|...))(.*)/));
assertEquals(["xxabcd", "xx", "abcd"], "xxabcd".match(/.*(?<=(xx|xxx))(.*)/));

// We do not backtrack into a lookbehind.
// The lookbehind captures "abc" so that \1 does not match. We do not backtrack
// to capture only "bc" in the lookbehind.
assertNull("abcdbc".match(/(?<=([abc]+)).\1/));

// Greedy loop.
assertEquals(["c", "bbbbbb"], "abbbbbbc".match(/(?<=(b+))c/));
assertEquals(["c", "b1234"], "ab1234c".match(/(?<=(b\d+))c/));
assertEquals(["c", "b12b23b34"], "ab12b23b34c".match(/(?<=((?:b\d{2})+))c/));

// Sticky
var re1 = /(?<=^(\w+))def/g;
assertEquals(["def", "abc"], re1.exec("abcdefdef"));
assertEquals(["def", "abcdef"], re1.exec("abcdefdef"));
var re2 = /\Bdef/g;
assertEquals(["def"], re2.exec("abcdefdef"));
assertEquals(["def"], re2.exec("abcdefdef"));

// Misc
assertNull("abcdef".match(/(?<=$abc)def/));
assertEquals(["foo"], "foo".match(/^foo(?<=foo)$/));
assertEquals(["foo"], "foo".match(/^f.o(?<=foo)$/));
assertNull("fno".match(/^f.o(?<=foo)$/));
assertNull("foo".match(/^foo(?<!foo)$/));
assertNull("foo".match(/^f.o(?<!foo)$/));
assertEquals(["fno"], "fno".match(/^f.o(?<!foo)$/));
assertEquals(["foooo"], "foooo".match(/^foooo(?<=fo+)$/));
assertEquals(["foooo"], "foooo".match(/^foooo(?<=fo*)$/));
assertEquals(["abc", "abc"], /(abc\1)/.exec("abc"));
assertEquals(["abc", "abc"], /(abc\1)/.exec("abc\u1234"));
assertEquals(["abc", "abc"], /(abc\1)/i.exec("abc"));
assertEquals(["abc", "abc"], /(abc\1)/i.exec("abc\u1234"));
var oob_subject = "abcdefghijklmnabcdefghijklmn".substr(14);
assertNull(oob_subject.match(/(?=(abcdefghijklmn))(?<=\1)a/i));
assertNull(oob_subject.match(/(?=(abcdefghijklmn))(?<=\1)a/));

// Mutual recursive capture/back references
assertEquals(["cacb", "a", ""], /(?<=a(.\2)b(\1)).{4}/.exec("aabcacbc"));
assertEquals(["b", "ac", "ac"], /(?<=a(\2)b(..\1))b/.exec("aacbacb"));
assertEquals(["x", "aa"], /(?<=(?:\1b)(aa))./.exec("aabaax"));
assertEquals(["x", "aa"], /(?<=(?:\1|b)(aa))./.exec("aaaax"));
