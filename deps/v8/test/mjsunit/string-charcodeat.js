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

/**
 * @fileoverview Check all sorts of borderline cases for charCodeAt.
 */

function Cons() {
  return "Te" + "st";
}


function Deep() {
  var a = "T";
  a += "e";
  a += "s";
  a += "t";
  return a;
}


function Slice() {
  return "testing Testing".substring(8, 12);
}


function Flat() {
  return "Test";
}

function Cons16() {
  return "Te" + "\u1234t";
}


function Deep16() {
  var a = "T";
  a += "e";
  a += "\u1234";
  a += "t";
  return a;
}


function Slice16Beginning() {
  return "Te\u1234t test".substring(0, 4);
}


function Slice16Middle() {
  return "test Te\u1234t test".substring(5, 9);
}


function Slice16End() {
  return "test Te\u1234t".substring(5, 9);
}


function Flat16() {
  return "Te\u1234t";
}


function Thing() {
}


function NotAString() {
  var n = new Thing();
  n.toString = function() { return "Test"; };
  n.charCodeAt = String.prototype.charCodeAt;
  return n;
}


function NotAString16() {
  var n = new Thing();
  n.toString = function() { return "Te\u1234t"; };
  n.charCodeAt = String.prototype.charCodeAt;
  return n;
}


function TestStringType(generator, sixteen) {
  var g = generator;
  assertTrue(isNaN(g().charCodeAt(-1e19)));
  assertTrue(isNaN(g().charCodeAt(-0x80000001)));
  assertTrue(isNaN(g().charCodeAt(-0x80000000)));
  assertTrue(isNaN(g().charCodeAt(-0x40000000)));
  assertTrue(isNaN(g().charCodeAt(-1)));
  assertTrue(isNaN(g().charCodeAt(4)));
  assertTrue(isNaN(g().charCodeAt(5)));
  assertTrue(isNaN(g().charCodeAt(0x3fffffff)));
  assertTrue(isNaN(g().charCodeAt(0x7fffffff)));
  assertTrue(isNaN(g().charCodeAt(0x80000000)));
  assertTrue(isNaN(g().charCodeAt(1e9)));
  assertEquals(84, g().charCodeAt(0));
  assertEquals(84, g().charCodeAt("test"));
  assertEquals(84, g().charCodeAt(""));
  assertEquals(84, g().charCodeAt(null));
  assertEquals(84, g().charCodeAt(undefined));
  assertEquals(84, g().charCodeAt());
  assertEquals(84, g().charCodeAt(void 0));
  assertEquals(84, g().charCodeAt(false));
  assertEquals(101, g().charCodeAt(true));
  assertEquals(101, g().charCodeAt(1));
  assertEquals(sixteen ? 0x1234 : 115, g().charCodeAt(2));
  assertEquals(116, g().charCodeAt(3));
  assertEquals(101, g().charCodeAt(1.1));
  assertEquals(sixteen ? 0x1234 : 115, g().charCodeAt(2.1718));
  assertEquals(116, g().charCodeAt(3.14159));
}


TestStringType(Cons, false);
TestStringType(Deep, false);
TestStringType(Slice, false);
TestStringType(Flat, false);
TestStringType(NotAString, false);
TestStringType(Cons16, true);
TestStringType(Deep16, true);
TestStringType(Slice16Beginning, true);
TestStringType(Slice16Middle, true);
TestStringType(Slice16End, true);
TestStringType(Flat16, true);
TestStringType(NotAString16, true);


function StupidThing() {
  // Doesn't return a string from toString!
  this.toString = function() { return 42; }
  this.charCodeAt = String.prototype.charCodeAt;
}

assertEquals(52, new StupidThing().charCodeAt(0));
assertEquals(50, new StupidThing().charCodeAt(1));
assertTrue(isNaN(new StupidThing().charCodeAt(2)));
assertTrue(isNaN(new StupidThing().charCodeAt(-1)));


// Medium (>255) and long (>65535) strings.

var medium = "12345678";
medium += medium; // 16.
medium += medium; // 32.
medium += medium; // 64.
medium += medium; // 128.
medium += medium; // 256.

var long = medium;
long += long + long + long;     // 1024.
long += long + long + long;     // 4096.
long += long + long + long;     // 16384.
long += long + long + long;     // 65536.

assertTrue(isNaN(medium.charCodeAt(-1)));
assertEquals(49, medium.charCodeAt(0));
assertEquals(56, medium.charCodeAt(255));
assertTrue(isNaN(medium.charCodeAt(256)));

assertTrue(isNaN(long.charCodeAt(-1)));
assertEquals(49, long.charCodeAt(0));
assertEquals(56, long.charCodeAt(65535));
assertTrue(isNaN(long.charCodeAt(65536)));
