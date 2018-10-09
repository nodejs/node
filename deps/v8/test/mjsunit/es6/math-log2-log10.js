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

// Flags: --allow-natives-syntax

[Math.log10, Math.log2].forEach( function(fun) {
  assertTrue(isNaN(fun(NaN)));
  assertTrue(isNaN(fun(fun)));
  assertTrue(isNaN(fun({ toString: function() { return NaN; } })));
  assertTrue(isNaN(fun({ valueOf: function() { return -1; } })));
  assertTrue(isNaN(fun({ valueOf: function() { return "abc"; } })));
  assertTrue(isNaN(fun(-0.1)));
  assertTrue(isNaN(fun(-1)));
  assertEquals("-Infinity", String(fun(0)));
  assertEquals("-Infinity", String(fun(-0)));
  assertEquals(0, fun(1));
  assertEquals("Infinity", String(fun(Infinity)));
});

for (var i = -310; i <= 308; i += 0.5) {
  assertEquals(i, Math.log10(Math.pow(10, i)));
  // Square roots are tested below.
  if (i != -0.5 && i != 0.5 ) {
    assertEqualsDelta(i, Math.log2(Math.pow(2, i)), Number.EPSILON);
  }
}

// Test denormals.
assertEquals(-307.77759430519706, Math.log10(1.5 * Math.pow(2, -1023)));

// Issue 4025.  Remove delta once issue 4029 has been fixed.
assertEqualsDelta(-9.643274665532873e-17, Math.log10(1-Number.EPSILON), 3e-32);

// Test Math.log2(2^k) for -1074 <= k <= 1023.
var n = -1074;
// This loop covers n from -1074 to -1043
for (var lowbits = 1; lowbits <= 0x80000000; lowbits *= 2) {
  var x = %ConstructDouble(0, lowbits);
  assertEquals(n, Math.log2(x));
  n++;
}
// This loop covers n from -1042 to -1023
for (var hibits = 1; hibits <= 0x80000; hibits *= 2) {
  var x = %ConstructDouble(hibits, 0);
  assertEquals(n, Math.log2(x));
  n++;
}
// The rest of the normal values of 2^n
var x = 1;
for (var n = -1022; n <= 1023; ++n) {
  var x = Math.pow(2, n);
  assertEquals(n, Math.log2(x));
}

// Test special values.
// Expectation isn't exactly 1/2 because Math.SQRT2 isn't exactly sqrt(2).
assertEquals(0.5000000000000001, Math.log2(Math.SQRT2));

// Expectation isn't exactly -1/2 because Math.SQRT1_2 isn't exactly sqrt(1/2).
assertEquals(-0.4999999999999999, Math.log2(Math.SQRT1_2));

assertEquals(3.321928094887362, Math.log2(10));
assertEquals(6.643856189774724, Math.log2(100));

// Test relationships
x = 1;
for (var k = 0; k < 1000; ++k) {
  var y = Math.abs(Math.log2(x) + Math.log2(1/x));
  assertEqualsDelta(0, y, 1.5e-14);
  x *= 1.1;
}

x = Math.pow(2, -100);
for (var k = 0; k < 1000; ++k) {
  var y = Math.log2(x);
  var expected = Math.log(x) / Math.LN2;
  var err = Math.abs(y - expected) / expected;
  assertEqualsDelta(0, err, 1e-15);
  x *= 1.1;
}
