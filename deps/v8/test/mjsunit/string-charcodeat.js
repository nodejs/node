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
 * @fileoverview Check all sorts of borderline cases for charCodeAt.
 */

function Cons() {
  return "Te" + "st testing 123";
}


function Deep() {
  var a = "T";
  a += "e";
  a += "s";
  a += "ting testing 123";
  return a;
}


function Slice() {
  return "testing Testing testing 123456789012345".substring(8, 22);
}


function Flat() {
  return "Testing testing 123";
}

function Cons16() {
  return "Te" + "\u1234t testing 123";
}


function Deep16() {
  var a = "T";
  a += "e";
  a += "\u1234";
  a += "ting testing 123";
  return a;
}


function Slice16Beginning() {
  return "Te\u1234t testing testing 123".substring(0, 14);
}


function Slice16Middle() {
  return "test Te\u1234t testing testing 123".substring(5, 19);
}


function Slice16End() {
  return "test Te\u1234t".substring(5, 9);
}


function Flat16() {
  return "Te\u1234ting testing 123";
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
  var len = g().toString().length;
  var t = sixteen ? "t" : "f"
  t += generator.name;
  assertTrue(isNaN(g().charCodeAt(-1e19)), 1 + t);
  assertTrue(isNaN(g().charCodeAt(-0x80000001)), 2 + t);
  assertTrue(isNaN(g().charCodeAt(-0x80000000)), 3 + t);
  assertTrue(isNaN(g().charCodeAt(-0x40000000)), 4 + t);
  assertTrue(isNaN(g().charCodeAt(-1)), 5 + t);
  assertTrue(isNaN(g().charCodeAt(len)), 6 + t);
  assertTrue(isNaN(g().charCodeAt(len + 1)), 7 + t);
  assertTrue(isNaN(g().charCodeAt(0x3fffffff)), 8 + t);
  assertTrue(isNaN(g().charCodeAt(0x7fffffff)), 9 + t);
  assertTrue(isNaN(g().charCodeAt(0x80000000)), 10 + t);
  assertTrue(isNaN(g().charCodeAt(1e9)), 11 + t);
  assertEquals(84, g().charCodeAt(0), 12 + t);
  assertEquals(84, g().charCodeAt("test"), 13 + t);
  assertEquals(84, g().charCodeAt(""), 14 + t);
  assertEquals(84, g().charCodeAt(null), 15 + t);
  assertEquals(84, g().charCodeAt(undefined), 16 + t);
  assertEquals(84, g().charCodeAt(), 17 + t);
  assertEquals(84, g().charCodeAt(void 0), 18 + t);
  assertEquals(84, g().charCodeAt(false), 19 + t);
  assertEquals(101, g().charCodeAt(true), 20 + t);
  assertEquals(101, g().charCodeAt(1), 21 + t);
  assertEquals(sixteen ? 0x1234 : 115, g().charCodeAt(2), 22 + t);
  assertEquals(116, g().charCodeAt(3), 23 + t);
  assertEquals(101, g().charCodeAt(1.1), 24 + t);
  assertEquals(sixteen ? 0x1234 : 115, g().charCodeAt(2.1718), 25 + t);
  assertEquals(116, g().charCodeAt(3.14159), 26 + t);
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

function Flat16Optimized() {
  var str = Flat16();
  return str.charCodeAt(2);
}

assertEquals(0x1234, Flat16Optimized());
assertEquals(0x1234, Flat16Optimized());
%OptimizeFunctionOnNextCall(Flat16Optimized);
assertEquals(0x1234, Flat16Optimized());

function ConsNotSmiIndex() {
  var str = Cons();
  assertTrue(isNaN(str.charCodeAt(0x7fffffff)));
}

for (var i = 0; i < 5; i++) {
  ConsNotSmiIndex();
}
%OptimizeFunctionOnNextCall(ConsNotSmiIndex);
ConsNotSmiIndex();


for (var i = 0; i != 10; i++) {
  assertEquals(101, Cons16().charCodeAt(1.1));
  assertEquals('e', Cons16().charAt(1.1));
}

function StupidThing() {
  // Doesn't return a string from toString!
  this.toString = function() { return 42; }
  this.charCodeAt = String.prototype.charCodeAt;
}

assertEquals(52, new StupidThing().charCodeAt(0), 27);
assertEquals(50, new StupidThing().charCodeAt(1), 28);
assertTrue(isNaN(new StupidThing().charCodeAt(2)), 29);
assertTrue(isNaN(new StupidThing().charCodeAt(-1)), 30);


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

assertTrue(isNaN(medium.charCodeAt(-1)), 31);
assertEquals(49, medium.charCodeAt(0), 32);
assertEquals(56, medium.charCodeAt(255), 33);
assertTrue(isNaN(medium.charCodeAt(256)), 34);

assertTrue(isNaN(long.charCodeAt(-1)), 35);
assertEquals(49, long.charCodeAt(0), 36);
assertEquals(56, long.charCodeAt(65535), 37);
assertTrue(isNaN(long.charCodeAt(65536)), 38);


// Test crankshaft code when the function is set directly on the
// string prototype object instead of the hidden prototype object.
// See http://code.google.com/p/v8/issues/detail?id=1070

String.prototype.x = String.prototype.charCodeAt;

function directlyOnPrototype() {
  assertEquals(97, "a".x(0));
  assertEquals(98, "b".x(0));
  assertEquals(99, "c".x(0));
  assertEquals(97, "a".x(0));
  assertEquals(98, "b".x(0));
  assertEquals(99, "c".x(0));
}

for (var i = 0; i < 5; i++) {
  directlyOnPrototype();
}
%OptimizeFunctionOnNextCall(directlyOnPrototype);
directlyOnPrototype();
