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


// Flags: --nodead-code-elimination --fold-constants --allow-natives-syntax --nostress-opt

function test(f) {
  f();
  f();
  %OptimizeFunctionOnNextCall(f);
  f();
  // Assert that there has been no deopt.
  assertOptimized(f);
}

test(function add() {
  assertEquals(2, 1 + 1);
  assertEquals(2.5, 1.25 + 1.25);
  assertEquals("Infinity", String(Infinity + Infinity));
  assertEquals("Infinity", String(Infinity + 3));
  assertEquals("NaN", String(Infinity + (-Infinity)));
  assertEquals("NaN", String(NaN + 2));
  assertEquals("-Infinity", String(1 / (-0.0 + (-0.0))));
  assertEquals("Infinity", String(1 / (-0.0 + 0.0)));
});

test(function inc() {
  var a = 1;
  var b = Infinity;
  var c = -Infinity;
  var d = NaN;
  assertEquals(2, ++a);
  assertEquals("Infinity", String(++b));
  assertEquals("-Infinity", String(++c));
  assertEquals("NaN", String(++d));
});

test(function dec() {
  var a = 1;
  var b = Infinity;
  var c = -Infinity;
  var d = NaN;
  assertEquals(0, --a);
  assertEquals("Infinity", String(--b));
  assertEquals("-Infinity", String(--c));
  assertEquals("NaN", String(--d));
});

test(function sub() {
  assertEquals(0, 1 - 1);
  assertEquals(0.5, 1.5 - 1);
  assertEquals("Infinity", String(Infinity - (-Infinity)));
  assertEquals("Infinity", String(Infinity - 3));
  assertEquals("NaN", String(Infinity - Infinity));
  assertEquals("NaN", String(NaN - 2));
  assertEquals("-Infinity", String(1 / (-0.0 - 0.0)));
  assertEquals("Infinity", String(1 / (0.0 - 0.0)));
});

test(function mul() {
  assertEquals(1, 1 * 1);
  assertEquals(2.25, 1.5 * 1.5);
  assertEquals("Infinity", String(Infinity * Infinity));
  assertEquals("-Infinity", String(Infinity * (-Infinity)));
  assertEquals("Infinity", String(Infinity * 3));
  assertEquals("-Infinity", String(Infinity * (-3)));
  assertEquals("NaN", String(NaN * 3));
  assertEquals("-Infinity", String(1 / (-0.0 * 0.0)));
  assertEquals("Infinity", String(1 / (0.0 * 0.0)));
});

test(function div() {
  assertEquals(1, 1 / 1);
  assertEquals(1.5, 2.25 / 1.5);
  assertEquals("NaN", String(Infinity / Infinity));
  assertEquals("Infinity", String(Infinity / 3));
  assertEquals("-Infinity", String(Infinity / (-3)));
  assertEquals("NaN", String(NaN / 3));
  assertEquals("-Infinity", String(1 / (-0.0)));
  assertEquals("Infinity", String(Infinity/0.0));
});

test(function mathMin() {
  assertEquals(1, Math.min(1, 10));
  assertEquals(1.5, Math.min(1.5, 2.5));
  assertEquals(0, Math.min(Infinity, 0));
  assertEquals("Infinity", String(Math.min(Infinity, Infinity)));
  assertEquals("-Infinity", String(Math.min(Infinity, -Infinity)));
  assertEquals("NaN", String(Math.min(NaN, 1)));
  assertEquals("Infinity", String(1 / Math.min(0.0, 0.0)));
  assertEquals("-Infinity", String(1 / Math.min(-0.0, -0.0)));
  assertEquals("-Infinity", String(1 / Math.min(0.0, -0.0)));
});

test(function mathMax() {
  assertEquals(10, Math.max(1, 10));
  assertEquals(2.5, Math.max(1.5, 2.5));
  assertEquals(Infinity, Math.max(Infinity, 0));
  assertEquals("-Infinity", String(Math.max(-Infinity, -Infinity)));
  assertEquals("Infinity", String(Math.max(Infinity, -Infinity)));
  assertEquals("NaN", String(Math.max(NaN, 1)));
  assertEquals("Infinity", String(1 / Math.max(0.0, 0.0)));
  assertEquals("-Infinity", String(1 / Math.max(-0.0, -0.0)));
  assertEquals("Infinity", String(1 / Math.max(0.0, -0.0)));
});

test(function mathExp() {
  assertEquals(1.0, Math.exp(0.0));
  assertTrue(2.7 < Math.exp(1) && Math.exp(1) < 2.8);
  assertEquals("Infinity", String(Math.exp(Infinity)));
  assertEquals("0", String(Math.exp(-Infinity)));
  assertEquals("NaN", String(Math.exp(NaN)));
});

test(function mathLog() {
  assertEquals(0.0, Math.log(1.0));
  assertTrue(1 < Math.log(3) && Math.log(3) < 1.5);
  assertEquals("Infinity", String(Math.log(Infinity)));
  assertEquals("NaN", String(Math.log(-Infinity)));
  assertEquals("NaN", String(Math.exp(NaN)));
});

test(function mathSqrt() {
  assertEquals(1.0, Math.sqrt(1.0));
  assertEquals("NaN", String(Math.sqrt(-1.0)));
  assertEquals("Infinity", String(Math.sqrt(Infinity)));
  assertEquals("NaN", String(Math.sqrt(-Infinity)));
  assertEquals("NaN", String(Math.sqrt(NaN)));
});

test(function mathPowHalf() {
  assertEquals(1.0, Math.pow(1.0, 0.5));
  assertEquals("NaN", String(Math.sqrt(-1.0)));
  assertEquals("Infinity", String(Math.pow(Infinity, 0.5)));
  assertEquals("NaN", String(Math.sqrt(-Infinity, 0.5)));
  assertEquals(0, Math.pow(Infinity, -0.5));
  assertEquals("NaN", String(Math.sqrt(-Infinity, -0.5)));
  assertEquals("NaN", String(Math.sqrt(NaN, 0.5)));
});

test(function mathAbs() {
  assertEquals(1.5, Math.abs(1.5));
  assertEquals(1.5, Math.abs(-1.5));
  assertEquals("Infinity", String(Math.abs(Infinity)));
  assertEquals("Infinity", String(Math.abs(-Infinity)));
  assertEquals("NaN", String(Math.abs(NaN)));
});

test(function mathRound() {
  assertEquals(2, Math.round(1.5));
  assertEquals(-1, Math.round(-1.5));
  assertEquals("Infinity", String(Math.round(Infinity)));
  assertEquals("-Infinity", String(Math.round(-Infinity)));
  assertEquals("Infinity", String(1 / Math.round(0.0)));
  assertEquals("-Infinity", String(1 / Math.round(-0.0)));
  assertEquals("NaN", String(Math.round(NaN)));
  assertEquals(Math.pow(2, 52) + 1, Math.round(Math.pow(2, 52) + 1));
});

test(function mathFround() {
  assertTrue(isNaN(Math.fround(NaN)));
  assertEquals("Infinity", String(1/Math.fround(0)));
  assertEquals("-Infinity", String(1/Math.fround(-0)));
  assertEquals("Infinity", String(Math.fround(Infinity)));
  assertEquals("-Infinity", String(Math.fround(-Infinity)));
  assertEquals("Infinity", String(Math.fround(1E200)));
  assertEquals("-Infinity", String(Math.fround(-1E200)));
  assertEquals(3.1415927410125732, Math.fround(Math.PI));
});

test(function mathFloor() {
  assertEquals(1, Math.floor(1.5));
  assertEquals(-2, Math.floor(-1.5));
  assertEquals("Infinity", String(Math.floor(Infinity)));
  assertEquals("-Infinity", String(Math.floor(-Infinity)));
  assertEquals("Infinity", String(1 / Math.floor(0.0)));
  assertEquals("-Infinity", String(1 / Math.floor(-0.0)));
  assertEquals("NaN", String(Math.floor(NaN)));
  assertEquals(Math.pow(2, 52) + 1, Math.floor(Math.pow(2, 52) + 1));
});

test(function mathPow() {
  assertEquals(2.25, Math.pow(1.5, 2));
  assertTrue(1.8 < Math.pow(1.5, 1.5) && Math.pow(1.5, 1.5) < 1.9);
  assertEquals("Infinity", String(Math.pow(Infinity, 0.5)));
  assertEquals("Infinity", String(Math.pow(-Infinity, 0.5)));
  assertEquals(0, Math.pow(Infinity, -0.5));
  assertEquals(0, Math.pow(Infinity, -0.5));
  assertEquals("Infinity", String(Math.pow(Infinity, Infinity)));
  assertEquals(0, Math.pow(Infinity, -Infinity));
  assertEquals("NaN", String(Math.pow(Infinity, NaN)));
  assertEquals("NaN", String(Math.pow(NaN, 2)));
});

test(function stringAdd() {
  assertEquals("", "" + "");
  assertEquals("folded constant", "folded " + "constant");
  assertEquals("not folded constant1", "not folded constant" + 1);
});

test(function stringLength() {
  assertEquals(6, "abcdef".length);
  assertEquals(0, "".length);
  assertEquals(-5, { length: -5 }.length);
});

test(function stringCharCodeAt() {
  assertEquals(99, "abc".charCodeAt(2));
  assertEquals("NaN", String("abc".charCodeAt(-1)));
  assertEquals("NaN", String("abc".charCodeAt(4)));
  assertEquals(98, "abc".charCodeAt(1.1));
  assertEquals("NaN", String("abc".charCodeAt(4.1)));
});

test(function stringCharAt() {
  assertEquals("c", "abc".charAt(2));
  assertEquals("", "abc".charAt(-1));
  assertEquals("", "abc".charAt(4));
  assertEquals("b", "abc".charAt(1.1));
  assertEquals("", "abc".charAt(4.1));
});


test(function int32Mod() {
  assertEquals(-0, -2147483648 % (-1));
});

test(function int32Div() {
  assertEquals(2147483648, -2147483648 / (-1));
});
