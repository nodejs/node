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

assertEquals(1, String.prototype.endsWith.length);

var testString = "Hello World";
assertTrue(testString.endsWith(""));
assertTrue(testString.endsWith("World"));
assertFalse(testString.endsWith("world"));
assertFalse(testString.endsWith("Hello World!"));
assertFalse(testString.endsWith(null));
assertFalse(testString.endsWith(undefined));

assertTrue("null".endsWith(null));
assertTrue("undefined".endsWith(undefined));

var georgianUnicodeString = "\u10D0\u10D1\u10D2\u10D3\u10D4\u10D5\u10D6\u10D7";
assertTrue(georgianUnicodeString.endsWith(georgianUnicodeString));
assertTrue(georgianUnicodeString.endsWith("\u10D4\u10D5\u10D6\u10D7"));
assertFalse(georgianUnicodeString.endsWith("\u10D0"));

assertThrows("String.prototype.endsWith.call(null, 'test')", TypeError);
assertThrows("String.prototype.endsWith.call(null, null)", TypeError);
assertThrows("String.prototype.endsWith.call(undefined, undefined)", TypeError);

assertThrows("String.prototype.endsWith.apply(null, ['test'])", TypeError);
assertThrows("String.prototype.endsWith.apply(null, [null])", TypeError);
assertThrows("String.prototype.endsWith.apply(undefined, [undefined])", TypeError);

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
  msg: "Regular expression /\d+/", val: /\d+/
}, {
  msg: "Empty array []", val: []
}, {
  msg: "Empty object {}", val: {}
}, {
  msg: "Array of size 3", val: new Array(3)
}];

function testNonStringValues() {
  var i = 0;
  var l = TEST_INPUT.length;

  for (; i < l; i++) {
    var e = TEST_INPUT[i];
    var v = e.val;
    var s = String(v);
    assertTrue(s.endsWith(v), e.msg);
    assertTrue(String.prototype.endsWith.call(v, v), e.msg);
    assertTrue(String.prototype.endsWith.apply(v, [v]), e.msg);
  }
}
testNonStringValues();

var CustomType = function(value) {
  this.endsWith = String.prototype.endsWith;
  this.toString = function() {
    return String(value);
  }
};

function testCutomType() {
  var i = 0;
  var l = TEST_INPUT.length;

  for (; i < l; i++) {
    var e = TEST_INPUT[i];
    var v = e.val;
    var o = new CustomType(v);
    assertTrue(o.endsWith(v), e.msg);
  }
}
testCutomType();


// Test cases found in FF
assertTrue("abc".endsWith("abc"));
assertTrue("abcd".endsWith("bcd"));
assertTrue("abc".endsWith("c"));
assertFalse("abc".endsWith("abcd"));
assertFalse("abc".endsWith("bbc"));
assertFalse("abc".endsWith("b"));
assertTrue("abc".endsWith("abc", 3));
assertTrue("abc".endsWith("bc", 3));
assertFalse("abc".endsWith("a", 3));
assertTrue("abc".endsWith("bc", 3));
assertTrue("abc".endsWith("a", 1));
assertFalse("abc".endsWith("abc", 1));
assertTrue("abc".endsWith("b", 2));
assertFalse("abc".endsWith("d", 2));
assertFalse("abc".endsWith("dcd", 2));
assertFalse("abc".endsWith("a", 42));
assertTrue("abc".endsWith("bc", Infinity));
assertFalse("abc".endsWith("a", Infinity));
assertTrue("abc".endsWith("bc", undefined));
assertFalse("abc".endsWith("bc", -43));
assertFalse("abc".endsWith("bc", -Infinity));
assertFalse("abc".endsWith("bc", NaN));
