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

assertEquals(1, String.prototype.includes.length);

var s = 'a';
assertFalse(s.includes(null));
assertFalse(s.includes(undefined));
assertFalse(s.includes());

var reString = "asdf[a-z]+(asdf)?";
assertTrue(reString.includes("[a-z]+"));
assertTrue(reString.includes("(asdf)?"));

// Random greek letters
var twoByteString = "\u039a\u0391\u03a3\u03a3\u0395";

// Test single char pattern
assertTrue(twoByteString.includes("\u039a"), "Lamda");
assertTrue(twoByteString.includes("\u0391"), "Alpha");
assertTrue(twoByteString.includes("\u03a3"), "First Sigma");
assertTrue(twoByteString.includes("\u03a3",3), "Second Sigma");
assertTrue(twoByteString.includes("\u0395"), "Epsilon");
assertFalse(twoByteString.includes("\u0392"), "Not beta");

// Test multi-char pattern
assertTrue(twoByteString.includes("\u039a\u0391"), "lambda Alpha");
assertTrue(twoByteString.includes("\u0391\u03a3"), "Alpha Sigma");
assertTrue(twoByteString.includes("\u03a3\u03a3"), "Sigma Sigma");
assertTrue(twoByteString.includes("\u03a3\u0395"), "Sigma Epsilon");

assertFalse(twoByteString.includes("\u0391\u03a3\u0395"),
    "Not Alpha Sigma Epsilon");

//single char pattern
assertTrue(twoByteString.includes("\u0395"));

assertThrows("String.prototype.includes.call(null, 'test')", TypeError);
assertThrows("String.prototype.includes.call(null, null)", TypeError);
assertThrows("String.prototype.includes.call(undefined, undefined)", TypeError);

assertThrows("String.prototype.includes.apply(null, ['test'])", TypeError);
assertThrows("String.prototype.includes.apply(null, [null])", TypeError);
assertThrows("String.prototype.includes.apply(undefined, [undefined])", TypeError);

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
  assertTrue(s.includes(v), e.msg);
  assertTrue(String.prototype.includes.call(v, v), e.msg);
  assertTrue(String.prototype.includes.apply(v, [v]), e.msg);
}

// Test cases found in FF
assertTrue("abc".includes("a"));
assertTrue("abc".includes("b"));
assertTrue("abc".includes("abc"));
assertTrue("abc".includes("bc"));
assertFalse("abc".includes("d"));
assertFalse("abc".includes("abcd"));
assertFalse("abc".includes("ac"));
assertTrue("abc".includes("abc", 0));
assertTrue("abc".includes("bc", 0));
assertFalse("abc".includes("de", 0));
assertTrue("abc".includes("bc", 1));
assertTrue("abc".includes("c", 1));
assertFalse("abc".includes("a", 1));
assertFalse("abc".includes("abc", 1));
assertTrue("abc".includes("c", 2));
assertFalse("abc".includes("d", 2));
assertFalse("abc".includes("dcd", 2));
assertFalse("abc".includes("a", 42));
assertFalse("abc".includes("a", Infinity));
assertTrue("abc".includes("ab", -43));
assertFalse("abc".includes("cd", -42));
assertTrue("abc".includes("ab", -Infinity));
assertFalse("abc".includes("cd", -Infinity));
assertTrue("abc".includes("ab", NaN));
assertFalse("abc".includes("cd", NaN));
assertFalse("xyzzy".includes("zy\0", 2));

var dots = Array(10000).join(".");
assertFalse(dots.includes("\x01", 10000));
assertFalse(dots.includes("\0", 10000));

var myobj = {
  toString: function () {
    return "abc";
  },
  includes: String.prototype.includes
};
assertTrue(myobj.includes("abc"));
assertFalse(myobj.includes("cd"));

var gotStr = false;
var gotPos = false;
myobj = {
  toString: function () {
    assertFalse(gotPos);
    gotStr = true;
    return "xyz";
  },
  includes: String.prototype.includes
};

assertEquals("foo[a-z]+(bar)?".includes("[a-z]+"), true);
assertThrows("'foo[a-z]+(bar)?'.includes(/[a-z]+/)", TypeError);
assertThrows("'foo/[a-z]+/(bar)?'.includes(/[a-z]+/)", TypeError);
assertEquals("foo[a-z]+(bar)?".includes("(bar)?"), true);
assertThrows("'foo[a-z]+(bar)?'.includes(/(bar)?/)", TypeError);
assertThrows("'foo[a-z]+/(bar)?/'.includes(/(bar)?/)", TypeError);

assertThrows("String.prototype.includes.call({ 'toString': function() { " +
  "throw RangeError(); } }, /./)", RangeError);
assertThrows("String.prototype.includes.call({ 'toString': function() { " +
  "return 'abc'; } }, /./)", TypeError);

assertThrows("String.prototype.includes.apply({ 'toString': function() { " +
  "throw RangeError(); } }, [/./])", RangeError);
assertThrows("String.prototype.includes.apply({ 'toString': function() { " +
  "return 'abc'; } }, [/./])", TypeError);

// includes does its brand checks with Symbol.match
var re = /./;
assertThrows(function() {
  "".includes(re);
}, TypeError);
re[Symbol.match] = false;
assertEquals(false, "".includes(re));
