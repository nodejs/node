// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-regexp-buffer-boundaries

// Tests for the regexp buffer-boundary assertions \A, \z, and \Z.
// https://github.com/tc39/proposal-regexp-buffer-boundaries

function assertMatchAt(re, subject, expected_index, expected_match) {
  re.lastIndex = 0;
  const match = re.exec(subject);
  assertNotNull(match);
  assertEquals(expected_match, match[0]);
  assertEquals(expected_index, match.index);
}

function assertNoMatch(re, subject) {
  re.lastIndex = 0;
  assertNull(re.exec(subject));
}

// ---------- \A: start of input ----------

assertMatchAt(/\Afoo/u, "foo", 0, "foo");
assertMatchAt(/\Afoo/u, "foobar", 0, "foo");
assertNoMatch(/\Afoo/u, "\nfoo");
assertNoMatch(/\Afoo/u, " foo");
assertNoMatch(/\Afoo/u, "xfoo");

// \A is unaffected by the m flag.
assertMatchAt(/\Afoo/um, "foo", 0, "foo");
assertNoMatch(/\Afoo/um, "\nfoo");

// Mixed with ^ in m mode: \A matches only at input start, ^ also matches
// after a line terminator.
assertMatchAt(/\Afoo|^bar/um, "foo", 0, "foo");
assertNoMatch(/\Afoo|^bar/um, "\nfoo");
assertMatchAt(/\Afoo|^bar/um, "bar", 0, "bar");
assertMatchAt(/\Afoo|^bar/um, "\nbar", 1, "bar");

// \A works with /v.
assertMatchAt(/\Afoo/v, "foo", 0, "foo");
assertNoMatch(/\Afoo/v, "xfoo");

// ---------- \z: end of input ----------

assertMatchAt(/foo\z/u, "foo", 0, "foo");
assertMatchAt(/foo\z/u, "\nfoo", 1, "foo");
assertNoMatch(/foo\z/u, "foo\n");
assertNoMatch(/foo\z/u, "foo\r\n");
assertNoMatch(/foo\z/u, "foo\u2028");
assertNoMatch(/foo\z/u, "foobar");

// \z is unaffected by the m flag.
assertMatchAt(/foo\z/um, "foo", 0, "foo");
assertNoMatch(/foo\z/um, "foo\nbar");

// /v.
assertMatchAt(/foo\z/v, "foo", 0, "foo");
assertNoMatch(/foo\z/v, "foo\n");

// ---------- \Z: end of input, optionally preceded by a line terminator -------

assertMatchAt(/end\Z/u, "The end", 4, "end");
assertMatchAt(/end\Z/u, "The end\n", 4, "end");
assertMatchAt(/end\Z/u, "The end\r", 4, "end");
assertMatchAt(/end\Z/u, "The end\r\n", 4, "end");
assertMatchAt(/end\Z/u, "The end\u2028", 4, "end");
assertMatchAt(/end\Z/u, "The end\u2029", 4, "end");
assertNoMatch(/end\Z/u, "The endx");
assertNoMatch(/end\Z/u, "The end\n...or is it?");
assertNoMatch(/end\Z/u, "The end\r\nX");
assertNoMatch(/end\Z/u, "The end\u2028X");

// \Z works with /v.
assertMatchAt(/end\Z/v, "The end", 4, "end");
assertMatchAt(/end\Z/v, "The end\r\n", 4, "end");
assertMatchAt(/end\Z/v, "The end\u2028", 4, "end");
assertNoMatch(/end\Z/v, "The end\r\nX");

// CRLF is atomic: for input "X\r\n" (len 3), \Z at position 1 matches via
// the "e == len-2 and input[e..e+1] == \r\n" branch.
assertMatchAt(/X\Z/u, "X\r\n", 0, "X");

// Verify match positions via replace. \Z is zero-width and matches at any
// of: (a) position len, (b) position len-1 when input[len-1] is a single
// LineTerminator, (c) position len-2 when input[len-2..] is "\r\n".
// For "ab\r\n" all three apply: \Z matches at positions 2, 3, and 4.
assertEquals("a|\n", "a\n".replace(/\Z/u, "|"));
assertEquals("a|\r", "a\r".replace(/\Z/u, "|"));
assertEquals("a|\r\n", "a\r\n".replace(/\Z/u, "|"));
assertEquals("ab|\r|\n|", "ab\r\n".replace(/\Z/gu, "|"));

// At end-of-input itself.
assertMatchAt(/\Z/u, "", 0, "");
assertMatchAt(/\Z/u, "foo", 3, "");
assertMatchAt(/\Z/u, "foo\n", 3, "");
assertMatchAt(/\Z/u, "\r\n", 0, "");

// \Z at position 0 of "\r\n": matches because e=0=len-2 and input is \r\n.
assertMatchAt(/^\Z/u, "\r\n", 0, "");
assertMatchAt(/^\Z/u, "\n", 0, "");
assertMatchAt(/^\Z/u, "\r", 0, "");
assertMatchAt(/^\Z/u, "", 0, "");
assertNoMatch(/^\Z/u, "\nX");
assertNoMatch(/^\Z/u, "\r\nX");

// \Z combined with capture.
{
  const m = /(e)nd\Z/u.exec("The end\n");
  assertNotNull(m);
  assertEquals("end", m[0]);
  assertEquals("e", m[1]);
  assertEquals(4, m.index);
}

// ---------- Mixed: \Afoo|^bar$|baz\z ----------
{
  const re = /\Afoo|^bar$|baz\z/um;
  assertMatchAt(re, "foo", 0, "foo");
  assertMatchAt(re, "foo\n", 0, "foo");
  assertNoMatch(re, "\nfoo");

  assertMatchAt(re, "bar", 0, "bar");
  assertMatchAt(re, "bar\n", 0, "bar");
  assertMatchAt(re, "\nbar", 1, "bar");

  assertMatchAt(re, "baz", 0, "baz");
  assertNoMatch(re, "baz\n");
  assertMatchAt(re, "\nbaz", 1, "baz");
}

// ---------- Inside character class: rejected under /u ----------

assertThrows(() => new RegExp("[\\A]", "u"), SyntaxError);
assertThrows(() => new RegExp("[\\z]", "u"), SyntaxError);
assertThrows(() => new RegExp("[\\Z]", "u"), SyntaxError);

assertThrows(() => new RegExp("[\\A]", "v"), SyntaxError);
assertThrows(() => new RegExp("[\\z]", "v"), SyntaxError);
assertThrows(() => new RegExp("[\\Z]", "v"), SyntaxError);

// ---------- Annex B: without /u or /v, \A \z \Z are IdentityEscape ----------

assertMatchAt(/\A/, "xA", 1, "A");
assertNoMatch(/\A/, "B");
assertNoMatch(/\A/, "");
assertMatchAt(/\z/, "xz", 1, "z");
assertNoMatch(/\z/, "y");
assertNoMatch(/\z/, "");
assertMatchAt(/\Z/, "xZ", 1, "Z");
assertNoMatch(/\Z/, "Y");
assertNoMatch(/\Z/, "");

// And inside a class, they're literal too.
assertMatchAt(/[\A]/, "xA", 1, "A");
assertNoMatch(/[\A]/, "x");
assertNoMatch(/[\A]/, "");
assertMatchAt(/[\z]/, "xz", 1, "z");
assertNoMatch(/[\z]/, "x");
assertNoMatch(/[\z]/, "");
assertMatchAt(/[\Z]/, "xZ", 1, "Z");
assertNoMatch(/[\Z]/, "x");
assertNoMatch(/[\Z]/, "");

// ---------- Quantifiers are not allowed on assertions ----------
// (Same as ^/$/\b in /u: assertions are not Atoms.)

assertThrows(() => new RegExp("\\A*", "u"), SyntaxError);
assertThrows(() => new RegExp("\\z*", "u"), SyntaxError);
assertThrows(() => new RegExp("\\Z*", "u"), SyntaxError);

// ---------- Match positions ----------

// \Afoo: match starts at 0.
assertMatchAt(/\Afoo/u, "foobar", 0, "foo");

// \Z on input ending with \r\n: match ends before the \r\n.
assertMatchAt(/\w+\Z/u, "abc\r\n", 0, "abc");

// \Z on input ending with single LT.
assertMatchAt(/\w+\Z/u, "abc\u2028", 0, "abc");

// \Z on input ending with neither: match at last word.
assertMatchAt(/\w+\Z/u, "abc", 0, "abc");

// ---------- contains_anchor hint: /\A.../u should not prefix-loop ----------

// Just check correctness: \A anchors the regex, so it cannot match elsewhere
// even if global.
const g = /\Aabc/gu;
assertMatchAt(g, "abcabc", 0, "abc");
assertNull(g.exec("abcabc"));
