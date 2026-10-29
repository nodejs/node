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

// Flags: --allow-natives-syntax

/**
 * @fileoverview Test indexing on strings with [].
 */

var foo = "Foo";
assertEquals("Foo", foo);
assertEquals("F", foo[0]);
assertEquals("o", foo[1]);
assertEquals("o", foo[2]);

// Test string keyed load IC.
for (var i = 0; i < 10; i++) {
  assertEquals("F", foo[0]);
  assertEquals("o", foo[1]);
  assertEquals("o", foo[2]);
  assertEquals("F", (foo[0] + "BarBazQuuxFooBarQuux")[0]);
}

assertEquals("F", foo["0" + ""], "string index");
assertEquals("o", foo["1"], "string index");
assertEquals("o", foo["2"], "string index");

assertEquals("undefined", typeof(foo[3]), "out of range");
// SpiderMonkey 1.5 fails this next one.  So does FF 2.0.6.
assertEquals("undefined", typeof(foo[-1]), "known failure in SpiderMonkey 1.5");
assertEquals("undefined", typeof(foo[-2]), "negative index");

foo[0] = "f";
assertEquals("Foo", foo);

foo[3] = "t";
assertEquals("Foo", foo);
assertEquals("undefined", typeof(foo[3]), "out of range");
assertEquals("undefined", typeof(foo[-2]), "negative index");

var S = new String("foo");
assertEquals(Object("foo"), S);
assertEquals("f", S[0], "string object");
assertEquals("f", S["0"], "string object");
S[0] = 'bente';
assertEquals("f", S[0], "string object");
assertEquals("f", S["0"], "string object");
S[-2] = 'spider';
assertEquals('spider', S[-2]);
S[3] = 'monkey';
assertEquals('monkey', S[3]);
S['foo'] = 'Fu';
assertEquals("Fu", S.foo);

// In FF this is ignored I think.  In V8 it puts a property on the String object
// but you won't ever see it because it is hidden by the 0th character in the
// string.  The net effect is pretty much the same.
S["0"] = 'bente';
assertEquals("f", S[0], "string object");
assertEquals("f", S["0"], "string object");

assertEquals(true, 0 in S, "0 in");
assertEquals(false, -1 in S, "-1 in");
assertEquals(true, 2 in S, "2 in");
assertEquals(true, 3 in S, "3 in");
assertEquals(false, 4 in S, "3 in");

assertEquals(true, "0" in S, '"0" in');
assertEquals(false, "-1" in S, '"-1" in');
assertEquals(true, "2" in S, '"2" in');
assertEquals(true, "3" in S, '"3" in');
assertEquals(false, "4" in S, '"3" in');

assertEquals(true, S.hasOwnProperty(0), "0 hasOwnProperty");
assertEquals(false, S.hasOwnProperty(-1), "-1 hasOwnProperty");
assertEquals(true, S.hasOwnProperty(2), "2 hasOwnProperty");
assertEquals(true, S.hasOwnProperty(3), "3 hasOwnProperty");
assertEquals(false, S.hasOwnProperty(4), "3 hasOwnProperty");

assertEquals(true, S.hasOwnProperty("0"), '"0" hasOwnProperty');
assertEquals(false, S.hasOwnProperty("-1"), '"-1" hasOwnProperty');
assertEquals(true, S.hasOwnProperty("2"), '"2" hasOwnProperty');
assertEquals(true, S.hasOwnProperty("3"), '"3" hasOwnProperty');
assertEquals(false, S.hasOwnProperty("4"), '"3" hasOwnProperty');

assertEquals(true, "foo".hasOwnProperty(0), "foo 0 hasOwnProperty");
assertEquals(false, "foo".hasOwnProperty(-1), "foo -1 hasOwnProperty");
assertEquals(true, "foo".hasOwnProperty(2), "foo 2 hasOwnProperty");
assertEquals(false, "foo".hasOwnProperty(4), "foo 3 hasOwnProperty");

assertEquals(true, "foo".hasOwnProperty("0"), 'foo "0" hasOwnProperty');
assertEquals(false, "foo".hasOwnProperty("-1"), 'foo "-1" hasOwnProperty');
assertEquals(true, "foo".hasOwnProperty("2"), 'foo "2" hasOwnProperty');
assertEquals(false, "foo".hasOwnProperty("4"), 'foo "3" hasOwnProperty');

//assertEquals(true, 0 in "foo", "0 in");
//assertEquals(false, -1 in "foo", "-1 in");
//assertEquals(true, 2 in "foo", "2 in");
//assertEquals(false, 3 in "foo", "3 in");
//
//assertEquals(true, "0" in "foo", '"0" in');
//assertEquals(false, "-1" in "foo", '"-1" in');
//assertEquals(true, "2" in "foo", '"2" in');
//assertEquals(false, "3" in "foo", '"3" in');

delete S[3];
assertEquals("undefined", typeof(S[3]));
assertEquals(false, 3 in S);
assertEquals(false, "3" in S);

var N = new Number(43);
assertEquals(Object(43), N);
N[-2] = "Alpha";
assertEquals("Alpha", N[-2]);
N[0] = "Zappa";
assertEquals("Zappa", N[0]);
assertEquals("Zappa", N["0"]);

var A = ["V", "e", "t", "t", "e", "r"];
var A2 = (A[0] = "v");
assertEquals('v', A[0]);
assertEquals('v', A2);

var S = new String("Onkel");
var S2 = (S[0] = 'o');
assertEquals('O', S[0]);
assertEquals('o', S2);

var s = "Tante";
var s2 = (s[0] = 't');
assertEquals('T', s[0]);
assertEquals('t', s2);

var S2 = (S[-2] = 'o');
assertEquals('o', S[-2]);
assertEquals('o', S2);

var s2 = (s[-2] = 't');
assertEquals('undefined', typeof(s[-2]));
assertEquals('t', s2);

// Make sure enough of the one-char string cache is filled.
var alpha = ['@'];
for (var i = 1; i < 128; i++) {
  var c = String.fromCharCode(i);
  alpha[i] = c[0];
}
var alphaStr = alpha.join("");

// Now test chars.
for (var i = 1; i < 128; i++) {
  assertEquals(alpha[i], alphaStr[i]);
  assertEquals(String.fromCharCode(i), alphaStr[i]);
}

// Test for keyed ic.
var foo = ['a12', ['a', 2, 'c'], 'a31', 42];
var results = [1, 2, 3, NaN];
for (var i = 0; i < 200; ++i) {
  var index = Math.floor(i / 50);
  var receiver = foo[index];
  var expected = results[index];
  var actual = +(receiver[1]);
  assertEquals(expected, actual);
}

var keys = [0, '1', 2, 3.0, -1, 10];
var str = 'abcd', arr = ['a', 'b', 'c', 'd', undefined, undefined];
for (var i = 0; i < 300; ++i) {
  var index = Math.floor(i / 50);
  var key = keys[index];
  var expected = arr[index];
  var actual = str[key];
  assertEquals(expected, actual);
}

// Test heap number case.
var keys = [0, Math.floor(2) * 0.5];
var str = 'ab', arr = ['a', 'b'];
for (var i = 0; i < 100; ++i) {
  var index = Math.floor(i / 50);
  var key = keys[index];
  var expected = arr[index];
  var actual = str[key];
  assertEquals(expected, actual);
}

// Test negative zero case.
var keys = [0, -0.0];
var str = 'ab', arr = ['a', 'a'];
for (var i = 0; i < 100; ++i) {
  var index = Math.floor(i / 50);
  var key = keys[index];
  var expected = arr[index];
  var actual = str[key];
  assertEquals(expected, actual);
}

// Test "not-an-array-index" case.
var keys = [0, 0.5];
var str = 'ab', arr = ['a', undefined];
for (var i = 0; i < 100; ++i) {
  var index = Math.floor(i / 50);
  var key = keys[index];
  var expected = arr[index];
  var actual = str[key];
  assertEquals(expected, actual);
}

// Test out of range case.
var keys = [0, -1];
var str = 'ab', arr = ['a', undefined];
for (var i = 0; i < 100; ++i) {
  var index = Math.floor(i / 50);
  var key = keys[index];
  var expected = arr[index];
  var actual = str[key];
  assertEquals(expected, actual);
}

var keys = [0, 10];
var str = 'ab', arr = ['a', undefined];
for (var i = 0; i < 100; ++i) {
  var index = Math.floor(i / 50);
  var key = keys[index];
  var expected = arr[index];
  var actual = str[key];
  assertEquals(expected, actual);
}

// Test out of range with a heap number case.
var num = Math.floor(4) * 0.5;
// TODO(mvstanton): figure out a reliable way to get a heap number every time.
// assertFalse(!%_IsSmi(num));
var keys = [0, num];
var str = 'ab', arr = ['a', undefined];
for (var i = 0; i < 100; ++i) {
  var index = Math.floor(i / 50);
  var key = keys[index];
  var expected = arr[index];
  var actual = str[key];
  assertEquals(expected, actual);
}

// Test two byte string.
var str = '\u0427', arr = ['\u0427'];
for (var i = 0; i < 50; ++i) {
  var expected = arr[0];
  var actual = str[0];
  assertEquals(expected, actual);
}
