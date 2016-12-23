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

var s = 'abcdefghijklmn';
assertEquals(s, s.substr());
assertEquals(s, s.substr(0));
assertEquals(s, s.substr('0'));
assertEquals(s, s.substr(void 0));
assertEquals(s, s.substr(null));
assertEquals(s, s.substr(false));
assertEquals(s, s.substr(0.9));
assertEquals(s, s.substr({ valueOf: function() { return 0; } }));
assertEquals(s, s.substr({ toString: function() { return '0'; } }));

var s1 = s.substring(1);
assertEquals(s1, s.substr(1));
assertEquals(s1, s.substr('1'));
assertEquals(s1, s.substr(true));
assertEquals(s1, s.substr(1.1));
assertEquals(s1, s.substr({ valueOf: function() { return 1; } }));
assertEquals(s1, s.substr({ toString: function() { return '1'; } }));


assertEquals(s.substring(s.length - 1), s.substr(-1));
assertEquals(s.substring(s.length - 1), s.substr(-1.2));
assertEquals(s.substring(s.length - 1), s.substr(-1.7));
assertEquals(s.substring(s.length - 2), s.substr(-2));
assertEquals(s.substring(s.length - 2), s.substr(-2.3));
assertEquals(s.substring(s.length - 2, s.length - 1), s.substr(-2, 1));
assertEquals(s, s.substr(-100));
assertEquals('abc', s.substr(-100, 3));
assertEquals(s1, s.substr(-s.length + 1));

// assertEquals('', s.substr(0, void 0)); // smjs and rhino
assertEquals('abcdefghijklmn', s.substr(0, void 0));  // kjs and v8
assertEquals('', s.substr(0, null));
assertEquals(s, s.substr(0, String(s.length)));
assertEquals('a', s.substr(0, true));


// Test substrings of different lengths and alignments.
// First ASCII.
var x = "ASCII";
for (var i = 0; i < 25; i++) {
  x += (i >> 4).toString(16) + (i & 0x0f).toString(16);
}
/x/.exec(x);  // Try to force a flatten.
for (var i = 5; i < 25; i++) {
  for (var j = 0; j < 25; j++) {
    var z = x.substring(i, i+j);
    var w = Math.random() * 42;  // Allocate something new in new-space.
    assertEquals(j, z.length);
    for (var k = 0; k < j; k++) {
      assertEquals(x.charAt(i+k), z.charAt(k));
    }
  }
}


// Then two-byte strings.
x = "UC16\u2028";  // Non-ascii char forces two-byte string.
for (var i = 0; i < 25; i++) {
  x += (i >> 4).toString(16) + (i & 0x0f).toString(16);
}
/x/.exec(x);  // Try to force a flatten.
for (var i = 5; i < 25; i++) {
  for (var j = 0; j < 25; j++) {
    var z = x.substring(i, i + j);
    var w = Math.random() * 42;  // Allocate something new in new-space.
    assertEquals(j, z.length);
    for (var k = 0; k < j; k++) {
      assertEquals(x.charAt(i+k), z.charAt(k));
    }
  }
}


// Keep creating strings to to force allocation failure on substring creation.
var x = "0123456789ABCDEF";
x += x;  // 2^5
x += x;
x += x;
x += x;
x += x;
x += x;  // 2^10
x += x;
x += x;
var xl = x.length;
var cache = [];
for (var i = 0; i < 10000; i++) {
  var z = x.substring(i % xl);
  assertEquals(xl - (i % xl), z.length);
  cache.push(z);
}


// Same with two-byte strings
var x = "\u2028123456789ABCDEF";
x += x;  // 2^5
x += x;
x += x;
x += x;
x += x;
x += x;  // 2^10
x += x;
x += x;
var xl = x.length;
var cache = [];
for (var i = 0; i < 10000; i++) {
  var z = x.substring(i % xl);
  assertEquals(xl - (i % xl), z.length);
  cache.push(z);
}

// Substring of substring.
var cache = [];
var last = x;
var offset = 0;
for (var i = 0; i < 64; i++) {
  var z = last.substring(i);
  last = z;
  cache.push(z);
  offset += i;
}
for (var i = 63; i >= 0; i--) {
  var z = cache.pop();
  assertTrue(/\u2028123456789ABCDEF/.test(z));
  assertEquals(xl - offset, z.length);
  offset -= i;
}


// Order of conversions.
{
  let log = [];
  let string = {[Symbol.toPrimitive]() { log.push("this"); return "abc" }};
  let start = {[Symbol.toPrimitive]() { log.push("start"); return 0 }};
  let length = {[Symbol.toPrimitive]() { log.push("length"); return 1 }};
  assertEquals("a", String.prototype.substr.call(string, start, length));
  assertEquals(["this", "start", "length"], log);
}
{
  let log = [];
  let string = {[Symbol.toPrimitive]() { log.push("this"); return "abc" }};
  let start = {[Symbol.toPrimitive]() { log.push("start"); return 0 }};
  let length = {[Symbol.toPrimitive]() { log.push("length"); return 0 }};
  assertEquals("", String.prototype.substr.call(string, start, length));
  assertEquals(["this", "start", "length"], log);
}

// Bounds edge cases.
{
  const str = "abc";
  const negativeHeapNumber = -1 * 2**32;
  const positiveHeapNumber = 2**32;

  assertEquals("abc", str.substr(negativeHeapNumber));
  assertEquals("abc", str.substr(negativeHeapNumber, str.length));
  assertEquals("abc", str.substr(-str.length, str.length));
  assertEquals("abc", str.substr(0, str.length));
  assertEquals("bc", str.substr(-2, str.length));
  assertEquals("c", str.substr(-1, str.length));

  assertEquals("", str.substr(str.length));
  assertEquals("", str.substr(4));
  assertEquals("", str.substr(positiveHeapNumber));

  assertEquals("abc", str.substr(negativeHeapNumber, positiveHeapNumber));
  assertEquals("abc", str.substr(negativeHeapNumber, positiveHeapNumber));
  assertEquals("abc", str.substr(-str.length, positiveHeapNumber));
  assertEquals("abc", str.substr(0, positiveHeapNumber));
  assertEquals("bc", str.substr(-2, positiveHeapNumber));
  assertEquals("c", str.substr(-1, positiveHeapNumber));

  assertEquals("", str.substr(str.length, positiveHeapNumber));
  assertEquals("", str.substr(4, positiveHeapNumber));
  assertEquals("", str.substr(positiveHeapNumber, positiveHeapNumber));

  assertEquals("", str.substr(negativeHeapNumber, negativeHeapNumber));
  assertEquals("", str.substr(negativeHeapNumber, negativeHeapNumber));
  assertEquals("", str.substr(-str.length, negativeHeapNumber));
  assertEquals("", str.substr(0, negativeHeapNumber));
  assertEquals("", str.substr(-2, negativeHeapNumber));
  assertEquals("", str.substr(-1, negativeHeapNumber));

  assertEquals("", str.substr(str.length, negativeHeapNumber));
  assertEquals("", str.substr(4, negativeHeapNumber));
  assertEquals("", str.substr(positiveHeapNumber, negativeHeapNumber));

  assertEquals("", str.substr(negativeHeapNumber, -1));
  assertEquals("", str.substr(negativeHeapNumber, -1));
  assertEquals("", str.substr(-str.length, -1));
  assertEquals("", str.substr(0, -1));
  assertEquals("", str.substr(-2, -1));
  assertEquals("", str.substr(-1, -1));

  assertEquals("", str.substr(str.length, -1));
  assertEquals("", str.substr(4, -1));
  assertEquals("", str.substr(positiveHeapNumber, -1));

  assertEquals("abc", str.substr(undefined));
  assertEquals("abc", str.substr(undefined, undefined));
}
