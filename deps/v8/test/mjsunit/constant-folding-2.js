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


// Flags: --allow-natives-syntax --nostress-opt --opt

function test(f, iterations) {
  %PrepareFunctionForOptimization(f);
  f();
  f();
  // Some of the tests need to learn until they stabilize.
  let n = iterations ? iterations : 1;
  for (let i = 0; i < n; i++) {
    %OptimizeFunctionOnNextCall(f);
    f();
  }
  // Assert that the function finally stabilized.
  assertOptimized(f);
}

test(function add() {
  assertEquals(2, 1 + 1);
  assertEquals(2.5, 1.25 + 1.25);
  assertSame(Infinity, Infinity + Infinity);
  assertSame(Infinity, Infinity + 3);
  assertSame(NaN, Infinity + (-Infinity));
  assertSame(NaN, NaN + 2);
  assertSame(-Infinity, 1 / (-0.0 + (-0.0)));
  assertSame(Infinity, 1 / (-0.0 + 0.0));
});

test(function inc() {
  var a = 1;
  var b = Infinity;
  var c = -Infinity;
  var d = NaN;
  assertEquals(2, ++a);
  assertSame(Infinity, ++b);
  assertSame(-Infinity, ++c);
  assertSame(NaN, ++d);
});

test(function dec() {
  var a = 1;
  var b = Infinity;
  var c = -Infinity;
  var d = NaN;
  assertEquals(0, --a);
  assertSame(Infinity, --b);
  assertSame(-Infinity, --c);
  assertSame(NaN, --d);
});

test(function sub() {
  assertEquals(0, 1 - 1);
  assertEquals(0.5, 1.5 - 1);
  assertSame(Infinity, Infinity - (-Infinity));
  assertSame(Infinity, Infinity - 3);
  assertSame(NaN, Infinity - Infinity);
  assertSame(NaN, NaN - 2);
  assertSame(-Infinity, 1 / (-0.0 - 0.0));
  assertSame(Infinity, 1 / (0.0 - 0.0));
});

test(function mul() {
  assertEquals(1, 1 * 1);
  assertEquals(2.25, 1.5 * 1.5);
  assertSame(Infinity, Infinity * Infinity);
  assertSame(-Infinity, Infinity * (-Infinity));
  assertSame(Infinity, Infinity * 3);
  assertSame(-Infinity, Infinity * (-3));
  assertSame(NaN, NaN * 3);
  assertSame(-Infinity, 1 / (-0.0 * 0.0));
  assertSame(Infinity, 1 / (0.0 * 0.0));
});

test(function div() {
  assertEquals(1, 1 / 1);
  assertEquals(1.5, 2.25 / 1.5);
  assertSame(NaN, Infinity / Infinity);
  assertSame(Infinity, Infinity / 3);
  assertSame(-Infinity, Infinity / (-3));
  assertSame(NaN, NaN / 3);
  assertSame(-Infinity, 1 / (-0.0));
  assertSame(Infinity, Infinity / 0.0);
});

test(function mathMin() {
  assertEquals(1, Math.min(1, 10));
  assertEquals(1.5, Math.min(1.5, 2.5));
  assertEquals(0, Math.min(Infinity, 0));
  assertSame(Infinity, Math.min(Infinity, Infinity));
  assertSame(-Infinity, Math.min(Infinity, -Infinity));
  assertSame(NaN, Math.min(NaN, 1));
  assertSame(Infinity, 1 / Math.min(0.0, 0.0));
  assertSame(-Infinity, 1 / Math.min(-0.0, -0.0));
  assertSame(-Infinity, 1 / Math.min(0.0, -0.0));
});

test(function mathMax() {
  assertEquals(10, Math.max(1, 10));
  assertEquals(2.5, Math.max(1.5, 2.5));
  assertEquals(Infinity, Math.max(Infinity, 0));
  assertSame(-Infinity, Math.max(-Infinity, -Infinity));
  assertSame(Infinity, Math.max(Infinity, -Infinity));
  assertSame(NaN, Math.max(NaN, 1));
  assertSame(Infinity, 1 / Math.max(0.0, 0.0));
  assertSame(-Infinity, 1 / Math.max(-0.0, -0.0));
  assertSame(Infinity, 1 / Math.max(0.0, -0.0));
});

test(function mathExp() {
  assertEquals(1.0, Math.exp(0.0));
  assertTrue(2.7 < Math.exp(1) && Math.exp(1) < 2.8);
  assertSame(Infinity, Math.exp(Infinity));
  assertEquals("0", String(Math.exp(-Infinity)));
  assertSame(NaN, Math.exp(NaN));
});

test(function mathLog() {
  assertEquals(0.0, Math.log(1.0));
  assertTrue(1 < Math.log(3) && Math.log(3) < 1.5);
  assertSame(Infinity, Math.log(Infinity));
  assertSame(NaN, Math.log(-Infinity));
  assertSame(NaN, Math.exp(NaN));
});

test(function mathSqrt() {
  assertEquals(1.0, Math.sqrt(1.0));
  assertSame(NaN, Math.sqrt(-1.0));
  assertSame(Infinity, Math.sqrt(Infinity));
  assertSame(NaN, Math.sqrt(-Infinity));
  assertSame(NaN, Math.sqrt(NaN));
});

test(function mathPowHalf() {
  assertEquals(1.0, Math.pow(1.0, 0.5));
  assertSame(NaN, Math.sqrt(-1.0));
  assertSame(Infinity, Math.pow(Infinity, 0.5));
  assertSame(NaN, Math.sqrt(-Infinity, 0.5));
  assertEquals(0, Math.pow(Infinity, -0.5));
  assertSame(NaN, Math.sqrt(-Infinity, -0.5));
  assertSame(NaN, Math.sqrt(NaN, 0.5));
});

test(function mathAbs() {
  assertEquals(1.5, Math.abs(1.5));
  assertEquals(1.5, Math.abs(-1.5));
  assertSame(Infinity, Math.abs(Infinity));
  assertSame(Infinity, Math.abs(-Infinity));
  assertSame(NaN, Math.abs(NaN));
});

test(function mathRound() {
  assertEquals(2, Math.round(1.5));
  assertEquals(-1, Math.round(-1.5));
  assertSame(Infinity, Math.round(Infinity));
  assertSame(-Infinity, Math.round(-Infinity));
  assertSame(Infinity, 1 / Math.round(0.0));
  assertSame(-Infinity, 1 / Math.round(-0.0));
  assertSame(NaN, Math.round(NaN));
  assertEquals(Math.pow(2, 52) + 1, Math.round(Math.pow(2, 52) + 1));
});

test(function mathFround() {
  assertTrue(isNaN(Math.fround(NaN)));
  assertSame(Infinity, 1 / Math.fround(0));
  assertSame(-Infinity, 1 / Math.fround(-0));
  assertSame(Infinity, Math.fround(Infinity));
  assertSame(-Infinity, Math.fround(-Infinity));
  assertSame(Infinity, Math.fround(1E200));
  assertSame(-Infinity, Math.fround(-1E200));
  assertEquals(3.1415927410125732, Math.fround(Math.PI));
});

test(function mathFloor() {
  assertEquals(1, Math.floor(1.5));
  assertEquals(-2, Math.floor(-1.5));
  assertSame(Infinity, Math.floor(Infinity));
  assertSame(-Infinity, Math.floor(-Infinity));
  assertSame(Infinity, 1 / Math.floor(0.0));
  assertSame(-Infinity, 1 / Math.floor(-0.0));
  assertSame(NaN, Math.floor(NaN));
  assertEquals(Math.pow(2, 52) + 1, Math.floor(Math.pow(2, 52) + 1));
});

test(function mathPow() {
  assertEquals(2.25, Math.pow(1.5, 2));
  assertTrue(1.8 < Math.pow(1.5, 1.5) && Math.pow(1.5, 1.5) < 1.9);
  assertSame(Infinity, Math.pow(Infinity, 0.5));
  assertSame(Infinity, Math.pow(-Infinity, 0.5));
  assertEquals(0, Math.pow(Infinity, -0.5));
  assertEquals(0, Math.pow(Infinity, -0.5));
  assertSame(Infinity, Math.pow(Infinity, Infinity));
  assertEquals(0, Math.pow(Infinity, -Infinity));
  assertSame(NaN, Math.pow(Infinity, NaN));
  assertSame(NaN, Math.pow(NaN, 2));
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

test(function stringCharAt() {
  assertEquals("c", "abc".charAt(2));
  assertEquals("", "abc".charAt(-1));
  assertEquals("", "abc".charAt(4));
  assertEquals("b", "abc".charAt(1.1));
  assertEquals("", "abc".charAt(4.1));
  assertEquals("", "abc".charAt(Infinity));
  assertEquals("", "abc".charAt(-Infinity));
  assertEquals("a", "abc".charAt(-0));
  assertEquals("a", "abc".charAt(+0));
  assertEquals("", "".charAt());
  assertEquals("", "abc".charAt(1 + 4294967295));
}, 10);

test(function stringCharCodeAt() {
  assertSame(99, "abc".charCodeAt(2));
  assertSame(NaN, "abc".charCodeAt(-1));
  assertSame(NaN, "abc".charCodeAt(4));
  assertSame(98, "abc".charCodeAt(1.1));
  assertSame(NaN, "abc".charCodeAt(4.1));
  assertSame(NaN, "abc".charCodeAt(Infinity));
  assertSame(NaN, "abc".charCodeAt(-Infinity));
  assertSame(97, "abc".charCodeAt(-0));
  assertSame(97, "abc".charCodeAt(+0));
  assertSame(NaN, "".charCodeAt());
  assertSame(NaN, "abc".charCodeAt(1 + 4294967295));
}, 10);

test(function stringCodePointAt() {
  assertSame(65533, "äϠ�𝌆".codePointAt(2));
  assertSame(119558, "äϠ�𝌆".codePointAt(3));
  assertSame(undefined, "äϠ�".codePointAt(-1));
  assertSame(undefined, "äϠ�".codePointAt(4));
  assertSame(992, "äϠ�".codePointAt(1.1));
  assertSame(undefined, "äϠ�".codePointAt(4.1));
  assertSame(undefined, "äϠ�".codePointAt(Infinity));
  assertSame(undefined, "äϠ�".codePointAt(-Infinity));
  assertSame(228, "äϠ�".codePointAt(-0));
  assertSame(97, "aϠ�".codePointAt(+0));
  assertSame(undefined, "".codePointAt());
  assertSame(undefined, "äϠ�".codePointAt(1 + 4294967295));
}, 10);

test(function stringFromCodePoint() {
  assertEquals(String.fromCodePoint(""), "\0");
  assertEquals(String.fromCodePoint(), "");
  assertEquals(String.fromCodePoint(-0), "\0");
  assertEquals(String.fromCodePoint(0), "\0");
  assertEquals(String.fromCodePoint(0x1D306), "\uD834\uDF06");
  assertEquals(
    String.fromCodePoint(0x1D306, 0x61, 0x1D307),
    "\uD834\uDF06a\uD834\uDF07");
  assertEquals(String.fromCodePoint(0x61, 0x62, 0x1D307), "ab\uD834\uDF07");
  assertEquals(String.fromCodePoint(false), "\0");
  assertEquals(String.fromCodePoint(null), "\0");
}, 5);

test(function stringFromCharCode() {
  assertEquals("！", String.fromCharCode(0x10FF01));
}, 2);

test(function int32Mod() {
  assertEquals(-0, -2147483648 % (-1));
});

test(function int32Div() {
  assertEquals(2147483648, -2147483648 / (-1));
});
