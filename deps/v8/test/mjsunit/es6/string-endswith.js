// Copyright 2014 the V8 project authors. All rights reserved.
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

// Test cases taken from
// https://github.com/mathiasbynens/String.prototype.endsWith/blob/master/tests/tests.js
Object.prototype[1] = 2; // try to break `arguments[1]`

assertEquals(String.prototype.endsWith.length, 1);
assertEquals(String.prototype.propertyIsEnumerable("endsWith"), false);

assertEquals("undefined".endsWith(), true);
assertEquals("undefined".endsWith(undefined), true);
assertEquals("undefined".endsWith(null), false);
assertEquals("null".endsWith(), false);
assertEquals("null".endsWith(undefined), false);
assertEquals("null".endsWith(null), true);

assertEquals("abc".endsWith(), false);
assertEquals("abc".endsWith(""), true);
assertEquals("abc".endsWith("\0"), false);
assertEquals("abc".endsWith("c"), true);
assertEquals("abc".endsWith("b"), false);
assertEquals("abc".endsWith("ab"), false);
assertEquals("abc".endsWith("bc"), true);
assertEquals("abc".endsWith("abc"), true);
assertEquals("abc".endsWith("bcd"), false);
assertEquals("abc".endsWith("abcd"), false);
assertEquals("abc".endsWith("bcde"), false);

assertEquals("abc".endsWith("", NaN), true);
assertEquals("abc".endsWith("\0", NaN), false);
assertEquals("abc".endsWith("c", NaN), false);
assertEquals("abc".endsWith("b", NaN), false);
assertEquals("abc".endsWith("ab", NaN), false);
assertEquals("abc".endsWith("bc", NaN), false);
assertEquals("abc".endsWith("abc", NaN), false);
assertEquals("abc".endsWith("bcd", NaN), false);
assertEquals("abc".endsWith("abcd", NaN), false);
assertEquals("abc".endsWith("bcde", NaN), false);

assertEquals("abc".endsWith("", false), true);
assertEquals("abc".endsWith("\0", false), false);
assertEquals("abc".endsWith("c", false), false);
assertEquals("abc".endsWith("b", false), false);
assertEquals("abc".endsWith("ab", false), false);
assertEquals("abc".endsWith("bc", false), false);
assertEquals("abc".endsWith("abc", false), false);
assertEquals("abc".endsWith("bcd", false), false);
assertEquals("abc".endsWith("abcd", false), false);
assertEquals("abc".endsWith("bcde", false), false);

assertEquals("abc".endsWith("", undefined), true);
assertEquals("abc".endsWith("\0", undefined), false);
assertEquals("abc".endsWith("c", undefined), true);
assertEquals("abc".endsWith("b", undefined), false);
assertEquals("abc".endsWith("ab", undefined), false);
assertEquals("abc".endsWith("bc", undefined), true);
assertEquals("abc".endsWith("abc", undefined), true);
assertEquals("abc".endsWith("bcd", undefined), false);
assertEquals("abc".endsWith("abcd", undefined), false);
assertEquals("abc".endsWith("bcde", undefined), false);

assertEquals("abc".endsWith("", null), true);
assertEquals("abc".endsWith("\0", null), false);
assertEquals("abc".endsWith("c", null), false);
assertEquals("abc".endsWith("b", null), false);
assertEquals("abc".endsWith("ab", null), false);
assertEquals("abc".endsWith("bc", null), false);
assertEquals("abc".endsWith("abc", null), false);
assertEquals("abc".endsWith("bcd", null), false);
assertEquals("abc".endsWith("abcd", null), false);
assertEquals("abc".endsWith("bcde", null), false);

assertEquals("abc".endsWith("", -Infinity), true);
assertEquals("abc".endsWith("\0", -Infinity), false);
assertEquals("abc".endsWith("c", -Infinity), false);
assertEquals("abc".endsWith("b", -Infinity), false);
assertEquals("abc".endsWith("ab", -Infinity), false);
assertEquals("abc".endsWith("bc", -Infinity), false);
assertEquals("abc".endsWith("abc", -Infinity), false);
assertEquals("abc".endsWith("bcd", -Infinity), false);
assertEquals("abc".endsWith("abcd", -Infinity), false);
assertEquals("abc".endsWith("bcde", -Infinity), false);

assertEquals("abc".endsWith("", -1), true);
assertEquals("abc".endsWith("\0", -1), false);
assertEquals("abc".endsWith("c", -1), false);
assertEquals("abc".endsWith("b", -1), false);
assertEquals("abc".endsWith("ab", -1), false);
assertEquals("abc".endsWith("bc", -1), false);
assertEquals("abc".endsWith("abc", -1), false);
assertEquals("abc".endsWith("bcd", -1), false);
assertEquals("abc".endsWith("abcd", -1), false);
assertEquals("abc".endsWith("bcde", -1), false);

assertEquals("abc".endsWith("", -0), true);
assertEquals("abc".endsWith("\0", -0), false);
assertEquals("abc".endsWith("c", -0), false);
assertEquals("abc".endsWith("b", -0), false);
assertEquals("abc".endsWith("ab", -0), false);
assertEquals("abc".endsWith("bc", -0), false);
assertEquals("abc".endsWith("abc", -0), false);
assertEquals("abc".endsWith("bcd", -0), false);
assertEquals("abc".endsWith("abcd", -0), false);
assertEquals("abc".endsWith("bcde", -0), false);

assertEquals("abc".endsWith("", +0), true);
assertEquals("abc".endsWith("\0", +0), false);
assertEquals("abc".endsWith("c", +0), false);
assertEquals("abc".endsWith("b", +0), false);
assertEquals("abc".endsWith("ab", +0), false);
assertEquals("abc".endsWith("bc", +0), false);
assertEquals("abc".endsWith("abc", +0), false);
assertEquals("abc".endsWith("bcd", +0), false);
assertEquals("abc".endsWith("abcd", +0), false);
assertEquals("abc".endsWith("bcde", +0), false);

assertEquals("abc".endsWith("", 1), true);
assertEquals("abc".endsWith("\0", 1), false);
assertEquals("abc".endsWith("c", 1), false);
assertEquals("abc".endsWith("b", 1), false);
assertEquals("abc".endsWith("ab", 1), false);
assertEquals("abc".endsWith("bc", 1), false);
assertEquals("abc".endsWith("abc", 1), false);
assertEquals("abc".endsWith("bcd", 1), false);
assertEquals("abc".endsWith("abcd", 1), false);
assertEquals("abc".endsWith("bcde", 1), false);

assertEquals("abc".endsWith("", 2), true);
assertEquals("abc".endsWith("\0", 2), false);
assertEquals("abc".endsWith("c", 2), false);
assertEquals("abc".endsWith("b", 2), true);
assertEquals("abc".endsWith("ab", 2), true);
assertEquals("abc".endsWith("bc", 2), false);
assertEquals("abc".endsWith("abc", 2), false);
assertEquals("abc".endsWith("bcd", 2), false);
assertEquals("abc".endsWith("abcd", 2), false);
assertEquals("abc".endsWith("bcde", 2), false);

assertEquals("abc".endsWith("", +Infinity), true);
assertEquals("abc".endsWith("\0", +Infinity), false);
assertEquals("abc".endsWith("c", +Infinity), true);
assertEquals("abc".endsWith("b", +Infinity), false);
assertEquals("abc".endsWith("ab", +Infinity), false);
assertEquals("abc".endsWith("bc", +Infinity), true);
assertEquals("abc".endsWith("abc", +Infinity), true);
assertEquals("abc".endsWith("bcd", +Infinity), false);
assertEquals("abc".endsWith("abcd", +Infinity), false);
assertEquals("abc".endsWith("bcde", +Infinity), false);

assertEquals("abc".endsWith("", true), true);
assertEquals("abc".endsWith("\0", true), false);
assertEquals("abc".endsWith("c", true), false);
assertEquals("abc".endsWith("b", true), false);
assertEquals("abc".endsWith("ab", true), false);
assertEquals("abc".endsWith("bc", true), false);
assertEquals("abc".endsWith("abc", true), false);
assertEquals("abc".endsWith("bcd", true), false);
assertEquals("abc".endsWith("abcd", true), false);
assertEquals("abc".endsWith("bcde", true), false);

assertEquals("abc".endsWith("", "x"), true);
assertEquals("abc".endsWith("\0", "x"), false);
assertEquals("abc".endsWith("c", "x"), false);
assertEquals("abc".endsWith("b", "x"), false);
assertEquals("abc".endsWith("ab", "x"), false);
assertEquals("abc".endsWith("bc", "x"), false);
assertEquals("abc".endsWith("abc", "x"), false);
assertEquals("abc".endsWith("bcd", "x"), false);
assertEquals("abc".endsWith("abcd", "x"), false);
assertEquals("abc".endsWith("bcde", "x"), false);

assertEquals("[a-z]+(bar)?".endsWith("(bar)?"), true);
assertThrows(function() { "[a-z]+(bar)?".endsWith(/(bar)?/);
}, TypeError);
assertEquals("[a-z]+(bar)?".endsWith("[a-z]+", 6), true);
assertThrows(function() { "[a-z]+(bar)?".endsWith(/(bar)?/);
}, TypeError);
assertThrows(function() { "[a-z]+/(bar)?/".endsWith(/(bar)?/);
}, TypeError);

// http://mathiasbynens.be/notes/javascript-unicode#poo-test
var string = "I\xF1t\xEBrn\xE2ti\xF4n\xE0liz\xE6ti\xF8n\u2603\uD83D\uDCA9";
assertEquals(string.endsWith(""), true);
assertEquals(string.endsWith("\xF1t\xEBr"), false);
assertEquals(string.endsWith("\xF1t\xEBr", 5), true);
assertEquals(string.endsWith("\xE0liz\xE6"), false);
assertEquals(string.endsWith("\xE0liz\xE6", 16), true);
assertEquals(string.endsWith("\xF8n\u2603\uD83D\uDCA9"), true);
assertEquals(string.endsWith("\xF8n\u2603\uD83D\uDCA9", 23), true);
assertEquals(string.endsWith("\u2603"), false);
assertEquals(string.endsWith("\u2603", 21), true);
assertEquals(string.endsWith("\uD83D\uDCA9"), true);
assertEquals(string.endsWith("\uD83D\uDCA9", 23), true);

assertThrows(function() {
  String.prototype.endsWith.call(undefined);
}, TypeError);
assertThrows(function() {
  String.prototype.endsWith.call(undefined, "b");
}, TypeError);
assertThrows(function() {
  String.prototype.endsWith.call(undefined, "b", 4);
}, TypeError);
assertThrows(function() {
  String.prototype.endsWith.call(null);
}, TypeError);
assertThrows(function() {
  String.prototype.endsWith.call(null, "b");
}, TypeError);
assertThrows(function() {
  String.prototype.endsWith.call(null, "b", 4);
}, TypeError);
assertEquals(String.prototype.endsWith.call(42, "2"), true);
assertEquals(String.prototype.endsWith.call(42, "4"), false);
assertEquals(String.prototype.endsWith.call(42, "b", 4), false);
assertEquals(String.prototype.endsWith.call(42, "2", 1), false);
assertEquals(String.prototype.endsWith.call(42, "2", 4), true);
assertEquals(String.prototype.endsWith.call({
  "toString": function() { return "abc"; }
}, "b", 0), false);
assertEquals(String.prototype.endsWith.call({
  "toString": function() { return "abc"; }
}, "b", 1), false);
assertEquals(String.prototype.endsWith.call({
  "toString": function() { return "abc"; }
}, "b", 2), true);
assertThrows(function() {
  String.prototype.endsWith.call({
    "toString": function() { throw RangeError(); }
  }, /./);
}, RangeError);
assertThrows(function() {
  String.prototype.endsWith.call({
    "toString": function() { return "abc"; }
  }, /./);
}, TypeError);

assertThrows(function() {
  String.prototype.endsWith.apply(undefined);
}, TypeError);
assertThrows(function() {
  String.prototype.endsWith.apply(undefined, ["b"]); },
TypeError);
assertThrows(function() {
  String.prototype.endsWith.apply(undefined, ["b", 4]);
}, TypeError);
assertThrows(function() {
  String.prototype.endsWith.apply(null);
}, TypeError);
assertThrows(function() {
  String.prototype.endsWith.apply(null, ["b"]);
}, TypeError);
assertThrows(function() {
  String.prototype.endsWith.apply(null, ["b", 4]);
}, TypeError);
assertEquals(String.prototype.endsWith.apply(42, ["2"]), true);
assertEquals(String.prototype.endsWith.apply(42, ["4"]), false);
assertEquals(String.prototype.endsWith.apply(42, ["b", 4]), false);
assertEquals(String.prototype.endsWith.apply(42, ["2", 1]), false);
assertEquals(String.prototype.endsWith.apply(42, ["2", 4]), true);
assertEquals(String.prototype.endsWith.apply({
  "toString": function() { return "abc"; }
}, ["b", 0]), false);
assertEquals(String.prototype.endsWith.apply({
  "toString": function() { return "abc"; }
}, ["b", 1]), false);
assertEquals(String.prototype.endsWith.apply({
  "toString": function() { return "abc"; }
}, ["b", 2]), true);
assertThrows(function() {
  String.prototype.endsWith.apply({
    "toString": function() { throw RangeError(); }
  }, [/./]);
}, RangeError);
assertThrows(function() {
  String.prototype.endsWith.apply({
    "toString": function() { return "abc"; }
  }, [/./]);
}, TypeError);

// endsWith does its brand checks with Symbol.match
var re = /./;
assertThrows(function() {
  "".startsWith(re);
}, TypeError);
re[Symbol.match] = false;
assertEquals(false, "".startsWith(re));
