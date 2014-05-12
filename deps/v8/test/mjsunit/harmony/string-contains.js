// Copyright 2013 the V8 project authors. All rights reserved.
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

// Flags: --harmony-strings

assertEquals(1, String.prototype.contains.length);

var reString = "asdf[a-z]+(asdf)?";
assertTrue(reString.contains("[a-z]+"));
assertTrue(reString.contains("(asdf)?"));

// Random greek letters
var twoByteString = "\u039a\u0391\u03a3\u03a3\u0395";

// Test single char pattern
assertTrue(twoByteString.contains("\u039a"), "Lamda");
assertTrue(twoByteString.contains("\u0391"), "Alpha");
assertTrue(twoByteString.contains("\u03a3"), "First Sigma");
assertTrue(twoByteString.contains("\u03a3",3), "Second Sigma");
assertTrue(twoByteString.contains("\u0395"), "Epsilon");
assertFalse(twoByteString.contains("\u0392"), "Not beta");

// Test multi-char pattern
assertTrue(twoByteString.contains("\u039a\u0391"), "lambda Alpha");
assertTrue(twoByteString.contains("\u0391\u03a3"), "Alpha Sigma");
assertTrue(twoByteString.contains("\u03a3\u03a3"), "Sigma Sigma");
assertTrue(twoByteString.contains("\u03a3\u0395"), "Sigma Epsilon");

assertFalse(twoByteString.contains("\u0391\u03a3\u0395"),
    "Not Alpha Sigma Epsilon");

//single char pattern
assertTrue(twoByteString.contains("\u0395"));

assertThrows("String.prototype.contains.call(null, 'test')", TypeError);
assertThrows("String.prototype.contains.call(null, null)", TypeError);
assertThrows("String.prototype.contains.call(undefined, undefined)", TypeError);

assertThrows("String.prototype.contains.apply(null, ['test'])", TypeError);
assertThrows("String.prototype.contains.apply(null, [null])", TypeError);
assertThrows("String.prototype.contains.apply(undefined, [undefined])", TypeError);

var TEST_INPUT = [{
  msg: "Empty string", val: ""
}, {
  msg: "Number 1234.34", val: 1234.34
}, {
  msg: "Integer number 0", val: 0
}, {
  msg: "Negative number -1", val: -1
}, {
  msg: "Boolean true", val: true
}, {
  msg: "Boolean false", val: false
}, {
  msg: "Empty array []", val: []
}, {
  msg: "Empty object {}", val: {}
}, {
  msg: "Array of size 3", val: new Array(3)
}];

var i = 0;
var l = TEST_INPUT.length;

for (; i < l; i++) {
  var e = TEST_INPUT[i];
  var v = e.val;
  var s = String(v);
  assertTrue(s.contains(v), e.msg);
  assertTrue(String.prototype.contains.call(v, v), e.msg);
  assertTrue(String.prototype.contains.apply(v, [v]), e.msg);
}

// Test cases found in FF
assertTrue("abc".contains("a"));
assertTrue("abc".contains("b"));
assertTrue("abc".contains("abc"));
assertTrue("abc".contains("bc"));
assertFalse("abc".contains("d"));
assertFalse("abc".contains("abcd"));
assertFalse("abc".contains("ac"));
assertTrue("abc".contains("abc", 0));
assertTrue("abc".contains("bc", 0));
assertFalse("abc".contains("de", 0));
assertTrue("abc".contains("bc", 1));
assertTrue("abc".contains("c", 1));
assertFalse("abc".contains("a", 1));
assertFalse("abc".contains("abc", 1));
assertTrue("abc".contains("c", 2));
assertFalse("abc".contains("d", 2));
assertFalse("abc".contains("dcd", 2));
assertFalse("abc".contains("a", 42));
assertFalse("abc".contains("a", Infinity));
assertTrue("abc".contains("ab", -43));
assertFalse("abc".contains("cd", -42));
assertTrue("abc".contains("ab", -Infinity));
assertFalse("abc".contains("cd", -Infinity));
assertTrue("abc".contains("ab", NaN));
assertFalse("abc".contains("cd", NaN));
assertFalse("xyzzy".contains("zy\0", 2));

var dots = Array(10000).join(".");
assertFalse(dots.contains("\x01", 10000));
assertFalse(dots.contains("\0", 10000));

var myobj = {
  toString: function () {
    return "abc";
  },
  contains: String.prototype.contains
};
assertTrue(myobj.contains("abc"));
assertFalse(myobj.contains("cd"));

var gotStr = false;
var gotPos = false;
myobj = {
  toString: function () {
    assertFalse(gotPos);
    gotStr = true;
    return "xyz";
  },
  contains: String.prototype.contains
};

assertEquals("foo[a-z]+(bar)?".contains("[a-z]+"), true);
assertThrows("'foo[a-z]+(bar)?'.contains(/[a-z]+/)", TypeError);
assertThrows("'foo/[a-z]+/(bar)?'.contains(/[a-z]+/)", TypeError);
assertEquals("foo[a-z]+(bar)?".contains("(bar)?"), true);
assertThrows("'foo[a-z]+(bar)?'.contains(/(bar)?/)", TypeError);
assertThrows("'foo[a-z]+/(bar)?/'.contains(/(bar)?/)", TypeError);

assertThrows("String.prototype.contains.call({ 'toString': function() { " +
  "throw RangeError(); } }, /./)", RangeError);
assertThrows("String.prototype.contains.call({ 'toString': function() { " +
  "return 'abc'; } }, /./)", TypeError);

assertThrows("String.prototype.contains.apply({ 'toString': function() { " +
  "throw RangeError(); } }, [/./])", RangeError);
assertThrows("String.prototype.contains.apply({ 'toString': function() { " +
  "return 'abc'; } }, [/./])", TypeError);
