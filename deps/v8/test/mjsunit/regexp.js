// Copyright 2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

function testEscape(str, regex) {
  assertEquals("foo:bar:baz", str.split(regex).join(":"));
}

testEscape("foo\nbar\nbaz", /\n/);
testEscape("foo bar baz", /\s/);
testEscape("foo\tbar\tbaz", /\s/);
testEscape("foo-bar-baz", /\u002D/);

// Test containing null char in regexp.
var s = '[' + String.fromCharCode(0) + ']';
var re = new RegExp(s);
assertEquals(s.match(re).length, 1);
assertEquals(s.match(re)[0], String.fromCharCode(0));

// Test strings containing all line separators
s = 'aA\nbB\rcC\r\ndD\u2028eE\u2029fF';
re = /^./gm; // any non-newline character at the beginning of a line
var result = s.match(re);
assertEquals(result.length, 6);
assertEquals(result[0], 'a');
assertEquals(result[1], 'b');
assertEquals(result[2], 'c');
assertEquals(result[3], 'd');
assertEquals(result[4], 'e');
assertEquals(result[5], 'f');

re = /.$/gm; // any non-newline character at the end of a line
result = s.match(re);
assertEquals(result.length, 6);
assertEquals(result[0], 'A');
assertEquals(result[1], 'B');
assertEquals(result[2], 'C');
assertEquals(result[3], 'D');
assertEquals(result[4], 'E');
assertEquals(result[5], 'F');

re = /^[^]/gm; // *any* character at the beginning of a line
result = s.match(re);
assertEquals(result.length, 7);
assertEquals(result[0], 'a');
assertEquals(result[1], 'b');
assertEquals(result[2], 'c');
assertEquals(result[3], '\n');
assertEquals(result[4], 'd');
assertEquals(result[5], 'e');
assertEquals(result[6], 'f');

re = /[^]$/gm; // *any* character at the end of a line
result = s.match(re);
assertEquals(result.length, 7);
assertEquals(result[0], 'A');
assertEquals(result[1], 'B');
assertEquals(result[2], 'C');
assertEquals(result[3], '\r');
assertEquals(result[4], 'D');
assertEquals(result[5], 'E');
assertEquals(result[6], 'F');

// Some tests from the Mozilla tests, where our behavior differs from
// SpiderMonkey.
// From ecma_3/RegExp/regress-334158.js
assertTrue(/\ca/.test( "\x01" ));
assertFalse(/\ca/.test( "\\ca" ));
// Passes in KJS, fails in IrregularExpressions.
// See http://code.google.com/p/v8/issues/detail?id=152
//assertTrue(/\c[a/]/.test( "\x1ba/]" ));


// Test \c in character class
re = /^[\cM]$/;
assertTrue(re.test("\r"));
assertFalse(re.test("M"));
assertFalse(re.test("c"));
assertFalse(re.test("\\"));
assertFalse(re.test("\x03"));  // I.e., read as \cc

re = /^[\c]]$/;
assertTrue(re.test("c]"));
assertFalse(re.test("\\]"));
assertFalse(re.test("\x1d"));  // ']' & 0x1f
assertFalse(re.test("\\]"));
assertFalse(re.test("\x03]"));  // I.e., read as \cc


// Test that we handle \s and \S correctly inside some bizarre
// character classes.
re = /[\s-:]/;
assertTrue(re.test('-'));
assertTrue(re.test(':'));
assertTrue(re.test(' '));
assertTrue(re.test('\t'));
assertTrue(re.test('\n'));
assertFalse(re.test('a'));
assertFalse(re.test('Z'));

re = /[\S-:]/;
assertTrue(re.test('-'));
assertTrue(re.test(':'));
assertFalse(re.test(' '));
assertFalse(re.test('\t'));
assertFalse(re.test('\n'));
assertTrue(re.test('a'));
assertTrue(re.test('Z'));

re = /[^\s-:]/;
assertFalse(re.test('-'));
assertFalse(re.test(':'));
assertFalse(re.test(' '));
assertFalse(re.test('\t'));
assertFalse(re.test('\n'));
assertTrue(re.test('a'));
assertTrue(re.test('Z'));

re = /[^\S-:]/;
assertFalse(re.test('-'));
assertFalse(re.test(':'));
assertTrue(re.test(' '));
assertTrue(re.test('\t'));
assertTrue(re.test('\n'));
assertFalse(re.test('a'));
assertFalse(re.test('Z'));

re = /[\s]/;
assertFalse(re.test('-'));
assertFalse(re.test(':'));
assertTrue(re.test(' '));
assertTrue(re.test('\t'));
assertTrue(re.test('\n'));
assertFalse(re.test('a'));
assertFalse(re.test('Z'));

re = /[^\s]/;
assertTrue(re.test('-'));
assertTrue(re.test(':'));
assertFalse(re.test(' '));
assertFalse(re.test('\t'));
assertFalse(re.test('\n'));
assertTrue(re.test('a'));
assertTrue(re.test('Z'));

re = /[\S]/;
assertTrue(re.test('-'));
assertTrue(re.test(':'));
assertFalse(re.test(' '));
assertFalse(re.test('\t'));
assertFalse(re.test('\n'));
assertTrue(re.test('a'));
assertTrue(re.test('Z'));

re = /[^\S]/;
assertFalse(re.test('-'));
assertFalse(re.test(':'));
assertTrue(re.test(' '));
assertTrue(re.test('\t'));
assertTrue(re.test('\n'));
assertFalse(re.test('a'));
assertFalse(re.test('Z'));

re = /[\s\S]/;
assertTrue(re.test('-'));
assertTrue(re.test(':'));
assertTrue(re.test(' '));
assertTrue(re.test('\t'));
assertTrue(re.test('\n'));
assertTrue(re.test('a'));
assertTrue(re.test('Z'));

re = /[^\s\S]/;
assertFalse(re.test('-'));
assertFalse(re.test(':'));
assertFalse(re.test(' '));
assertFalse(re.test('\t'));
assertFalse(re.test('\n'));
assertFalse(re.test('a'));
assertFalse(re.test('Z'));

// Test beginning and end of line assertions with or without the
// multiline flag.
re = /^\d+/;
assertFalse(re.test("asdf\n123"));
re = /^\d+/m;
assertTrue(re.test("asdf\n123"));

re = /\d+$/;
assertFalse(re.test("123\nasdf"));
re = /\d+$/m;
assertTrue(re.test("123\nasdf"));

// Test that empty matches are handled correctly for multiline global
// regexps.
re = /^(.*)/mg;
assertEquals(3, "a\n\rb".match(re).length);
assertEquals("*a\n*b\r*c\n*\r*d\r*\n*e", "a\nb\rc\n\rd\r\ne".replace(re, "*$1"));

// Test that empty matches advance one character
re = new RegExp("", "g");
assertEquals("xAx", "A".replace(re, "x"));
assertEquals(3, String.fromCharCode(161).replace(re, "x").length);

// Test that we match the KJS behavior with regard to undefined constructor
// arguments:
re = new RegExp();
// KJS actually shows this as '//'.  Here we match the Firefox behavior (ie,
// giving a syntactically legal regexp literal).
assertEquals('/(?:)/', re.toString());
re = new RegExp(void 0);
assertEquals('/(?:)/', re.toString());
re.compile();
assertEquals('/(?:)/', re.toString());
re.compile(void 0);
assertEquals('/undefined/', re.toString());


// Check for lazy RegExp literal creation
function lazyLiteral(doit) {
  if (doit) return "".replace(/foo(/gi, "");
  return true;
}

assertTrue(lazyLiteral(false));
assertThrows("lazyLiteral(true)");

// Check $01 and $10
re = new RegExp("(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)");
assertEquals("t", "123456789t".replace(re, "$10"), "$10");
assertEquals("15", "123456789t".replace(re, "$15"), "$10");
assertEquals("1", "123456789t".replace(re, "$01"), "$01");
assertEquals("$001", "123456789t".replace(re, "$001"), "$001");
re = new RegExp("foo(.)");
assertEquals("bar$0", "foox".replace(re, "bar$0"), "$0");
assertEquals("bar$00", "foox".replace(re, "bar$00"), "$00");
assertEquals("bar$000", "foox".replace(re, "bar$000"), "$000");
assertEquals("barx", "foox".replace(re, "bar$01"), "$01 2");
assertEquals("barx5", "foox".replace(re, "bar$15"), "$15");

assertFalse(/()foo$\1/.test("football"), "football1");
assertFalse(/foo$(?=ball)/.test("football"), "football2");
assertFalse(/foo$(?!bar)/.test("football"), "football3");
assertTrue(/()foo$\1/.test("foo"), "football4");
assertTrue(/foo$(?=(ball)?)/.test("foo"), "football5");
assertTrue(/()foo$(?!bar)/.test("foo"), "football6");
assertFalse(/(x?)foo$\1/.test("football"), "football7");
assertFalse(/foo$(?=ball)/.test("football"), "football8");
assertFalse(/foo$(?!bar)/.test("football"), "football9");
assertTrue(/(x?)foo$\1/.test("foo"), "football10");
assertTrue(/foo$(?=(ball)?)/.test("foo"), "football11");
assertTrue(/foo$(?!bar)/.test("foo"), "football12");

// Check that the back reference has two successors.  See
// BackReferenceNode::PropagateForward.
assertFalse(/f(o)\b\1/.test('foo'));
assertTrue(/f(o)\B\1/.test('foo'));

// Back-reference, ignore case:
// ASCII
assertEquals("xaAx,a", String(/x(a)\1x/i.exec("xaAx")), "backref-ASCII");
assertFalse(/x(...)\1/i.test("xaaaaa"), "backref-ASCII-short");
assertTrue(/x((?:))\1\1x/i.test("xx"), "backref-ASCII-empty");
assertTrue(/x(?:...|(...))\1x/i.test("xabcx"), "backref-ASCII-uncaptured");
assertTrue(/x(?:...|(...))\1x/i.test("xabcABCx"), "backref-ASCII-backtrack");
assertEquals("xaBcAbCABCx,aBc",
             String(/x(...)\1\1x/i.exec("xaBcAbCABCx")),
             "backref-ASCII-twice");

for (var i = 0; i < 128; i++) {
  var testName = "backref-ASCII-char-" + i + "," + (i^0x20);
  var test = /^(.)\1$/i.test(String.fromCharCode(i, i ^ 0x20))
  var c = String.fromCharCode(i);
  if (('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z')) {
    assertTrue(test, testName);
  } else {
    assertFalse(test, testName);
  }
}

assertFalse(/f(o)$\1/.test('foo'), "backref detects at_end");

// Check decimal escapes doesn't overflow.
// (Note: \214 is interpreted as octal).
assertEquals(/\2147483648/.exec("\x8c7483648"),
             ["\x8c7483648"],
             "Overflow decimal escape");


// Check numbers in quantifiers doesn't overflow and doesn't throw on
// too large numbers.
assertFalse(/a{111111111111111111111111111111111111111111111}/.test('b'),
            "overlarge1");
assertFalse(/a{999999999999999999999999999999999999999999999}/.test('b'),
            "overlarge2");
assertFalse(/a{1,111111111111111111111111111111111111111111111}/.test('b'),
            "overlarge3");
assertFalse(/a{1,999999999999999999999999999999999999999999999}/.test('b'),
            "overlarge4");
assertFalse(/a{2147483648}/.test('b'),
            "overlarge5");
assertFalse(/a{21474836471}/.test('b'),
            "overlarge6");
assertFalse(/a{1,2147483648}/.test('b'),
            "overlarge7");
assertFalse(/a{1,21474836471}/.test('b'),
            "overlarge8");
assertFalse(/a{2147483648,2147483648}/.test('b'),
            "overlarge9");
assertFalse(/a{21474836471,21474836471}/.test('b'),
            "overlarge10");
assertFalse(/a{2147483647}/.test('b'),
            "overlarge11");
assertFalse(/a{1,2147483647}/.test('b'),
            "overlarge12");
assertTrue(/a{1,2147483647}/.test('a'),
            "overlarge13");
assertFalse(/a{2147483647,2147483647}/.test('a'),
            "overlarge14");


// Check that we don't read past the end of the string.
assertFalse(/f/.test('b'));
assertFalse(/[abc]f/.test('x'));
assertFalse(/[abc]f/.test('xa'));
assertFalse(/[abc]</.test('x'));
assertFalse(/[abc]</.test('xa'));
assertFalse(/f/i.test('b'));
assertFalse(/[abc]f/i.test('x'));
assertFalse(/[abc]f/i.test('xa'));
assertFalse(/[abc]</i.test('x'));
assertFalse(/[abc]</i.test('xa'));
assertFalse(/f[abc]/.test('x'));
assertFalse(/f[abc]/.test('xa'));
assertFalse(/<[abc]/.test('x'));
assertFalse(/<[abc]/.test('xa'));
assertFalse(/f[abc]/i.test('x'));
assertFalse(/f[abc]/i.test('xa'));
assertFalse(/<[abc]/i.test('x'));
assertFalse(/<[abc]/i.test('xa'));

// Test that merging of quick test masks gets it right.
assertFalse(/x([0-7]%%x|[0-6]%%y)/.test('x7%%y'), 'qt');
assertFalse(/()x\1(y([0-7]%%%x|[0-6]%%%y)|dkjasldkas)/.test('xy7%%%y'), 'qt2');
assertFalse(/()x\1(y([0-7]%%%x|[0-6]%%%y)|dkjasldkas)/.test('xy%%%y'), 'qt3');
assertFalse(/()x\1y([0-7]%%%x|[0-6]%%%y)/.test('xy7%%%y'), 'qt4');
assertFalse(/()x\1(y([0-7]%%%x|[0-6]%%%y)|dkjasldkas)/.test('xy%%%y'), 'qt5');
assertFalse(/()x\1y([0-7]%%%x|[0-6]%%%y)/.test('xy7%%%y'), 'qt6');
assertFalse(/xy([0-7]%%%x|[0-6]%%%y)/.test('xy7%%%y'), 'qt7');
assertFalse(/x([0-7]%%%x|[0-6]%%%y)/.test('x7%%%y'), 'qt8');


// Don't hang on this one.
/[^\xfe-\xff]*/.test("");


var long = "a";
for (var i = 0; i < 100000; i++) {
  long = "a?" + long;
}
// Don't crash on this one, but maybe throw an exception.
try {
  RegExp(long).exec("a");
} catch (e) {
  assertTrue(String(e).indexOf("Stack overflow") >= 0, "overflow");
}


// Test that compile works on modified objects
var re = /re+/;
assertEquals("re+", re.source);
assertFalse(re.global);
assertFalse(re.ignoreCase);
assertFalse(re.multiline);
assertEquals(0, re.lastIndex);

re.compile("ro+", "gim");
assertEquals("ro+", re.source);
assertTrue(re.global);
assertTrue(re.ignoreCase);
assertTrue(re.multiline);
assertEquals(0, re.lastIndex);

re.lastIndex = 42;
re.someOtherProperty = 42;
re.someDeletableProperty = 42;
re[37] = 37;  
re[42] = 42;  

re.compile("ra+", "i");
assertEquals("ra+", re.source);
assertFalse(re.global);
assertTrue(re.ignoreCase);
assertFalse(re.multiline);
assertEquals(0, re.lastIndex);

assertEquals(42, re.someOtherProperty);
assertEquals(42, re.someDeletableProperty);
assertEquals(37, re[37]);
assertEquals(42, re[42]);

re.lastIndex = -1;
re.someOtherProperty = 37;
re[42] = 37;
assertTrue(delete re[37]);
assertTrue(delete re.someDeletableProperty);
re.compile("ri+", "gm");

assertEquals("ri+", re.source);
assertTrue(re.global);
assertFalse(re.ignoreCase);
assertTrue(re.multiline);
assertEquals(0, re.lastIndex);
assertEquals(37, re.someOtherProperty);
assertEquals(37, re[42]);

// Test boundary-checks.
function assertRegExpTest(re, input, test) { 
  assertEquals(test, re.test(input), "test:" + re + ":" + input);
}

assertRegExpTest(/b\b/, "b", true);
assertRegExpTest(/b\b$/, "b", true);
assertRegExpTest(/\bb/, "b", true);
assertRegExpTest(/^\bb/, "b", true);
assertRegExpTest(/,\b/, ",", false);
assertRegExpTest(/,\b$/, ",", false);
assertRegExpTest(/\b,/, ",", false);
assertRegExpTest(/^\b,/, ",", false);

assertRegExpTest(/b\B/, "b", false);
assertRegExpTest(/b\B$/, "b", false);
assertRegExpTest(/\Bb/, "b", false);
assertRegExpTest(/^\Bb/, "b", false);
assertRegExpTest(/,\B/, ",", true);
assertRegExpTest(/,\B$/, ",", true);
assertRegExpTest(/\B,/, ",", true);
assertRegExpTest(/^\B,/, ",", true);

assertRegExpTest(/b\b/, "b,", true);
assertRegExpTest(/b\b/, "ba", false);
assertRegExpTest(/b\B/, "b,", false);
assertRegExpTest(/b\B/, "ba", true);

assertRegExpTest(/b\Bb/, "bb", true);
assertRegExpTest(/b\bb/, "bb", false);

assertRegExpTest(/b\b[,b]/, "bb", false);
assertRegExpTest(/b\B[,b]/, "bb", true);
assertRegExpTest(/b\b[,b]/, "b,", true);
assertRegExpTest(/b\B[,b]/, "b,", false);

assertRegExpTest(/[,b]\bb/, "bb", false);
assertRegExpTest(/[,b]\Bb/, "bb", true);
assertRegExpTest(/[,b]\bb/, ",b", true);
assertRegExpTest(/[,b]\Bb/, ",b", false);

assertRegExpTest(/[,b]\b[,b]/, "bb", false);
assertRegExpTest(/[,b]\B[,b]/, "bb", true);
assertRegExpTest(/[,b]\b[,b]/, ",b", true);
assertRegExpTest(/[,b]\B[,b]/, ",b", false);
assertRegExpTest(/[,b]\b[,b]/, "b,", true);
assertRegExpTest(/[,b]\B[,b]/, "b,", false);
