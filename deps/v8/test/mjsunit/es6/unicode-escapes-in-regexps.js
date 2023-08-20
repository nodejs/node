// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ES6 extends the \uxxxx escape and also allows \u{xxxxx}.

function testRegexpHelper(r) {
  assertTrue(r.test("foo"));
  assertTrue(r.test("boo"));
  assertFalse(r.test("moo"));
}


(function TestUnicodeEscapes() {
  testRegexpHelper(/(\u0066|\u0062)oo/);
  testRegexpHelper(/(\u0066|\u0062)oo/u);
  testRegexpHelper(/(\u{0066}|\u{0062})oo/u);
  testRegexpHelper(/(\u{66}|\u{000062})oo/u);

  // Note that we need \\ inside a string, otherwise it's interpreted as a
  // unicode escape inside a string.
  testRegexpHelper(new RegExp("(\\u0066|\\u0062)oo"));
  testRegexpHelper(new RegExp("(\\u0066|\\u0062)oo", "u"));
  testRegexpHelper(new RegExp("(\\u{0066}|\\u{0062})oo", "u"));
  testRegexpHelper(new RegExp("(\\u{66}|\\u{000062})oo", "u"));

  // Though, unicode escapes via strings should work too.
  testRegexpHelper(new RegExp("(\u0066|\u0062)oo"));
  testRegexpHelper(new RegExp("(\u0066|\u0062)oo", "u"));
  testRegexpHelper(new RegExp("(\u{0066}|\u{0062})oo", "u"));
  testRegexpHelper(new RegExp("(\u{66}|\u{000062})oo", "u"));
})();


(function TestUnicodeEscapesInCharacterClasses() {
  testRegexpHelper(/[\u0062-\u0066]oo/);
  testRegexpHelper(/[\u0062-\u0066]oo/u);
  testRegexpHelper(/[\u{0062}-\u{0066}]oo/u);
  testRegexpHelper(/[\u{62}-\u{00000066}]oo/u);

  // Note that we need \\ inside a string, otherwise it's interpreted as a
  // unicode escape inside a string.
  testRegexpHelper(new RegExp("[\\u0062-\\u0066]oo"));
  testRegexpHelper(new RegExp("[\\u0062-\\u0066]oo", "u"));
  testRegexpHelper(new RegExp("[\\u{0062}-\\u{0066}]oo", "u"));
  testRegexpHelper(new RegExp("[\\u{62}-\\u{00000066}]oo", "u"));

  // Though, unicode escapes via strings should work too.
  testRegexpHelper(new RegExp("[\u0062-\u0066]oo"));
  testRegexpHelper(new RegExp("[\u0062-\u0066]oo", "u"));
  testRegexpHelper(new RegExp("[\u{0062}-\u{0066}]oo", "u"));
  testRegexpHelper(new RegExp("[\u{62}-\u{00000066}]oo", "u"));
})();


(function TestBraceEscapesWithoutUnicodeFlag() {
  // \u followed by illegal escape will be parsed as u. {x} will be the
  // character count.
  function helper1(r) {
    assertFalse(r.test("fbar"));
    assertFalse(r.test("fubar"));
    assertTrue(r.test("fuubar"));
    assertFalse(r.test("fuuubar"));
  }
  helper1(/f\u{2}bar/);
  helper1(new RegExp("f\\u{2}bar"));

  function helper2(r) {
    assertFalse(r.test("fbar"));
    assertTrue(r.test("fubar"));
    assertTrue(r.test("fuubar"));
    assertFalse(r.test("fuuubar"));
  }

  helper2(/f\u{1,2}bar/);
  helper2(new RegExp("f\\u{1,2}bar"));

  function helper3(r) {
    assertTrue(r.test("u"));
    assertTrue(r.test("{"));
    assertTrue(r.test("2"));
    assertTrue(r.test("}"));
    assertFalse(r.test("q"));
    assertFalse(r.test("("));
    assertFalse(r.test(")"));
  }
  helper3(/[\u{2}]/);
  helper3(new RegExp("[\\u{2}]"));
})();


(function TestInvalidEscapes() {
  // Without the u flag, invalid unicode escapes and other invalid escapes are
  // treated as identity escapes.
  function helper1(r) {
    assertTrue(r.test("firstuxz89second"));
  }
  helper1(/first\u\x\z\8\9second/);
  helper1(new RegExp("first\\u\\x\\z\\8\\9second"));

  function helper2(r) {
    assertTrue(r.test("u"));
    assertTrue(r.test("x"));
    assertTrue(r.test("z"));
    assertTrue(r.test("8"));
    assertTrue(r.test("9"));
    assertFalse(r.test("q"));
    assertFalse(r.test("7"));
  }
  helper2(/[\u\x\z\8\9]/);
  helper2(new RegExp("[\\u\\x\\z\\8\\9]"));

  // However, with the u flag, these are treated as invalid escapes.
  assertThrows("/\\u/u", SyntaxError);
  assertThrows("/\\u12/u", SyntaxError);
  assertThrows("/\\ufoo/u", SyntaxError);
  assertThrows("/\\x/u", SyntaxError);
  assertThrows("/\\xfoo/u", SyntaxError);
  assertThrows("/\\z/u", SyntaxError);
  assertThrows("/\\8/u", SyntaxError);
  assertThrows("/\\9/u", SyntaxError);

  assertThrows("new RegExp('\\\\u', 'u')", SyntaxError);
  assertThrows("new RegExp('\\\\u12', 'u')", SyntaxError);
  assertThrows("new RegExp('\\\\ufoo', 'u')", SyntaxError);
  assertThrows("new RegExp('\\\\x', 'u')", SyntaxError);
  assertThrows("new RegExp('\\\\xfoo', 'u')", SyntaxError);
  assertThrows("new RegExp('\\\\z', 'u')", SyntaxError);
  assertThrows("new RegExp('\\\\8', 'u')", SyntaxError);
  assertThrows("new RegExp('\\\\9', 'u')", SyntaxError);
})();


(function TestTooBigHexEscape() {
  // The hex number inside \u{} has a maximum value.
  /\u{10ffff}/u
  new RegExp("\\u{10ffff}", "u")
  assertThrows("/\\u{110000}/u", SyntaxError);
  assertThrows("new RegExp('\\\\u{110000}', 'u')", SyntaxError);

  // Without the u flag, they're of course fine ({x} is the count).
  /\u{110000}/
  new RegExp("\\u{110000}")
})();


(function TestSyntaxEscapes() {
  // Syntax escapes work the same with or without the u flag.
  function helper(r) {
    assertTrue(r.test("foo[bar"));
    assertFalse(r.test("foo]bar"));
  }
  helper(/foo\[bar/);
  helper(new RegExp("foo\\[bar"));
  helper(/foo\[bar/u);
  helper(new RegExp("foo\\[bar", "u"));
})();


(function TestUnicodeSurrogates() {
  // U+10E6D corresponds to the surrogate pair [U+D803, U+DE6D].
  function helper(r) {
    assertTrue(r.test("foo\u{10e6d}bar"));
  }
  helper(/foo\ud803\ude6dbar/u);
  helper(new RegExp("foo\\ud803\\ude6dbar", "u"));
})();


(function AllFlags() {
  // Test that we can pass all possible regexp flags and they work properly.
  function helper1(r) {
    assertTrue(r.global);
    assertTrue(r.ignoreCase);
    assertTrue(r.multiline);
    assertTrue(r.sticky);
    assertTrue(r.unicode);
  }

  helper1(/foo/gimyu);
  helper1(new RegExp("foo", "gimyu"));

  function helper2(r) {
    assertFalse(r.global);
    assertFalse(r.ignoreCase);
    assertFalse(r.multiline);
    assertFalse(r.sticky);
    assertFalse(r.unicode);
  }

  helper2(/foo/);
  helper2(new RegExp("foo"));
})();


(function DuplicatedFlags() {
  // Test that duplicating the u flag is not allowed.
  assertThrows("/foo/ugu");
  assertThrows("new RegExp('foo', 'ugu')");
})();


(function ToString() {
  // Test that the u flag is included in the string representation of regexps.
  function helper(r) {
    assertEquals(r.toString(), "/foo/u");
  }
  helper(/foo/u);
  helper(new RegExp("foo", "u"));
})();

// Non-BMP patterns.
// Single character atom.
assertTrue(new RegExp("\u{12345}", "u").test("\u{12345}"));
assertTrue(/\u{12345}/u.test("\u{12345}"));
assertTrue(new RegExp("\u{12345}", "u").test("\ud808\udf45"));
assertTrue(/\u{12345}/u.test("\ud808\udf45"));
assertFalse(new RegExp("\u{12345}", "u").test("\udf45"));
assertFalse(/\u{12345}/u.test("\udf45"));

// Multi-character atom.
assertTrue(new RegExp("\u{12345}\u{23456}", "u").test("a\u{12345}\u{23456}b"));
assertTrue(/\u{12345}\u{23456}/u.test("b\u{12345}\u{23456}c"));
assertFalse(new RegExp("\u{12345}\u{23456}", "u").test("a\udf45\u{23456}b"));
assertFalse(/\u{12345}\u{23456}/u.test("b\udf45\u{23456}c"));

// Disjunction.
assertTrue(new RegExp("\u{12345}(?:\u{23456})", "u").test(
    "a\u{12345}\u{23456}b"));
assertTrue(/\u{12345}(?:\u{23456})/u.test("b\u{12345}\u{23456}c"));
assertFalse(new RegExp("\u{12345}(?:\u{23456})", "u").test(
    "a\udf45\u{23456}b"));
assertFalse(/\u{12345}(?:\u{23456})/u.test("b\udf45\u{23456}c"));

// Alternative.
assertTrue(new RegExp("\u{12345}|\u{23456}", "u").test("a\u{12345}b"));
assertTrue(/\u{12345}|\u{23456}/u.test("b\u{23456}c"));
assertFalse(new RegExp("\u{12345}|\u{23456}", "u").test("a\udf45\ud84db"));
assertFalse(/\u{12345}|\u{23456}/u.test("b\udf45\ud808c"));

// Capture.
assertTrue(new RegExp("(\u{12345}|\u{23456}).\\1", "u").test(
    "\u{12345}b\u{12345}"));
assertTrue(/(\u{12345}|\u{23456}).\1/u.test("\u{12345}b\u{12345}"));
assertFalse(new RegExp("(\u{12345}|\u{23456}).\\1", "u").test(
    "\u{12345}b\u{23456}"));
assertFalse(/(\u{12345}|\u{23456}).\1/u.test("\u{12345}b\u{23456}"));

// Quantifier.
assertTrue(new RegExp("\u{12345}{3}", "u").test("\u{12345}\u{12345}\u{12345}"));
assertTrue(/\u{12345}{3}/u.test("\u{12345}\u{12345}\u{12345}"));
assertTrue(new RegExp("\u{12345}{3}").test("\u{12345}\udf45\udf45"));
assertFalse(/\ud808\udf45{3}/u.test("\u{12345}\udf45\udf45"));
assertTrue(/\ud808\udf45{3}/u.test("\u{12345}\u{12345}\u{12345}"));
assertFalse(new RegExp("\u{12345}{3}", "u").test("\u{12345}\udf45\udf45"));
assertFalse(/\u{12345}{3}/u.test("\u{12345}\udf45\udf45"));

// Literal surrogates.
assertEquals(["\u{10000}\u{10000}"],
             new RegExp("\ud800\udc00+", "u").exec("\u{10000}\u{10000}"));
assertEquals(["\u{10000}\u{10000}"],
             new RegExp("\\ud800\\udc00+", "u").exec("\u{10000}\u{10000}"));

assertEquals(["\u{10003}\u{50001}"],
             new RegExp("[\\ud800\\udc03-\\ud900\\udc01\]+", "u").exec(
                 "\u{10003}\u{50001}"));
assertEquals(["\u{10003}\u{50001}"],
             new RegExp("[\ud800\udc03-\u{50001}\]+", "u").exec(
                 "\u{10003}\u{50001}"));

// Unicode escape sequences to represent a non-BMP character cannot have
// mixed notation, and must follow the rules for RegExpUnicodeEscapeSequence.
assertThrows(() => new RegExp("[\\ud800\udc03-\ud900\\udc01\]+", "u"));
assertThrows(() => new RegExp("[\\ud800\udc03-\ud900\\udc01\]+", "u"));
assertNull(new RegExp("\\ud800\udc00+", "u").exec("\u{10000}\u{10000}"));
assertNull(new RegExp("\ud800\\udc00+", "u").exec("\u{10000}\u{10000}"));

assertNull(new RegExp("[\\ud800\udc00]", "u").exec("\u{10000}"));
assertNull(new RegExp("[\\{ud800}\udc00]", "u").exec("\u{10000}"));
assertNull(new RegExp("[\ud800\\udc00]", "u").exec("\u{10000}"));
assertNull(new RegExp("[\ud800\\{udc00}]", "u").exec("\u{10000}"));

assertNull(/\u{d800}\u{dc00}+/u.exec("\ud800\udc00\udc00"));
assertNull(/\ud800\u{dc00}+/u.exec("\ud800\udc00\udc00"));
assertNull(/\u{d800}\udc00+/u.exec("\ud800\udc00\udc00"));
