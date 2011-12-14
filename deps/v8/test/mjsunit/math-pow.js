// Copyright 2010 the V8 project authors. All rights reserved.
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
// Tests the special cases specified by ES 15.8.2.13

function test() {
  // Simple sanity check
  assertEquals(4, Math.pow(2, 2));
  assertEquals(2147483648, Math.pow(2, 31));
  assertEquals(0.25, Math.pow(2, -2));
  assertEquals(0.0625, Math.pow(2, -4));
  assertEquals(1, Math.pow(1, 100));
  assertEquals(0, Math.pow(0, 1000));

  // Spec tests
  assertEquals(NaN, Math.pow(2, NaN));
  assertEquals(NaN, Math.pow(+0, NaN));
  assertEquals(NaN, Math.pow(-0, NaN));
  assertEquals(NaN, Math.pow(Infinity, NaN));
  assertEquals(NaN, Math.pow(-Infinity, NaN));

  assertEquals(1, Math.pow(NaN, +0));
  assertEquals(1, Math.pow(NaN, -0));

  assertEquals(NaN, Math.pow(NaN, NaN));
  assertEquals(NaN, Math.pow(NaN, 2.2));
  assertEquals(NaN, Math.pow(NaN, 1));
  assertEquals(NaN, Math.pow(NaN, -1));
  assertEquals(NaN, Math.pow(NaN, -2.2));
  assertEquals(NaN, Math.pow(NaN, Infinity));
  assertEquals(NaN, Math.pow(NaN, -Infinity));

  assertEquals(Infinity, Math.pow(1.1, Infinity));
  assertEquals(Infinity, Math.pow(-1.1, Infinity));
  assertEquals(Infinity, Math.pow(2, Infinity));
  assertEquals(Infinity, Math.pow(-2, Infinity));

  // Because +0 == -0, we need to compare 1/{+,-}0 to {+,-}Infinity
  assertEquals(+Infinity, 1/Math.pow(1.1, -Infinity));
  assertEquals(+Infinity, 1/Math.pow(-1.1, -Infinity));
  assertEquals(+Infinity, 1/Math.pow(2, -Infinity));
  assertEquals(+Infinity, 1/Math.pow(-2, -Infinity));

  assertEquals(NaN, Math.pow(1, Infinity));
  assertEquals(NaN, Math.pow(1, -Infinity));
  assertEquals(NaN, Math.pow(-1, Infinity));
  assertEquals(NaN, Math.pow(-1, -Infinity));

  assertEquals(+0, Math.pow(0.1, Infinity));
  assertEquals(+0, Math.pow(-0.1, Infinity));
  assertEquals(+0, Math.pow(0.999, Infinity));
  assertEquals(+0, Math.pow(-0.999, Infinity));

  assertEquals(Infinity, Math.pow(0.1, -Infinity));
  assertEquals(Infinity, Math.pow(-0.1, -Infinity));
  assertEquals(Infinity, Math.pow(0.999, -Infinity));
  assertEquals(Infinity, Math.pow(-0.999, -Infinity));

  assertEquals(Infinity, Math.pow(Infinity, 0.1));
  assertEquals(Infinity, Math.pow(Infinity, 2));

  assertEquals(+Infinity, 1/Math.pow(Infinity, -0.1));
  assertEquals(+Infinity, 1/Math.pow(Infinity, -2));

  assertEquals(-Infinity, Math.pow(-Infinity, 3));
  assertEquals(-Infinity, Math.pow(-Infinity, 13));

  assertEquals(Infinity, Math.pow(-Infinity, 3.1));
  assertEquals(Infinity, Math.pow(-Infinity, 2));

  assertEquals(-Infinity, 1/Math.pow(-Infinity, -3));
  assertEquals(-Infinity, 1/Math.pow(-Infinity, -13));

  assertEquals(+Infinity, 1/Math.pow(-Infinity, -3.1));
  assertEquals(+Infinity, 1/Math.pow(-Infinity, -2));

  assertEquals(+Infinity, 1/Math.pow(+0, 1.1));
  assertEquals(+Infinity, 1/Math.pow(+0, 2));

  assertEquals(Infinity, Math.pow(+0, -1.1));
  assertEquals(Infinity, Math.pow(+0, -2));

  assertEquals(-Infinity, 1/Math.pow(-0, 3));
  assertEquals(-Infinity, 1/Math.pow(-0, 13));

  assertEquals(+Infinity, 1/Math.pow(-0, 3.1));
  assertEquals(+Infinity, 1/Math.pow(-0, 2));

  assertEquals(-Infinity, Math.pow(-0, -3));
  assertEquals(-Infinity, Math.pow(-0, -13));

  assertEquals(Infinity, Math.pow(-0, -3.1));
  assertEquals(Infinity, Math.pow(-0, -2));

  assertEquals(NaN, Math.pow(-0.00001, 1.1));
  assertEquals(NaN, Math.pow(-0.00001, -1.1));
  assertEquals(NaN, Math.pow(-1.1, 1.1));
  assertEquals(NaN, Math.pow(-1.1, -1.1));
  assertEquals(NaN, Math.pow(-2, 1.1));
  assertEquals(NaN, Math.pow(-2, -1.1));
  assertEquals(NaN, Math.pow(-1000, 1.1));
  assertEquals(NaN, Math.pow(-1000, -1.1));

  assertEquals(+Infinity, 1/Math.pow(-0, 0.5));
  assertEquals(+Infinity, 1/Math.pow(-0, 0.6));
  assertEquals(-Infinity, 1/Math.pow(-0, 1));
  assertEquals(-Infinity, 1/Math.pow(-0, 10000000001));

  assertEquals(+Infinity, Math.pow(-0, -0.5));
  assertEquals(+Infinity, Math.pow(-0, -0.6));
  assertEquals(-Infinity, Math.pow(-0, -1));
  assertEquals(-Infinity, Math.pow(-0, -10000000001));

  assertEquals(4, Math.pow(16, 0.5));
  assertEquals(NaN, Math.pow(-16, 0.5));
  assertEquals(0.25, Math.pow(16, -0.5));
  assertEquals(NaN, Math.pow(-16, -0.5));

  // Test detecting and converting integer value as double.
  assertEquals(8, Math.pow(2, Math.sqrt(9)));

  // Tests from Mozilla 15.8.2.13.
  assertEquals(2, Math.pow.length);
  assertEquals(NaN, Math.pow());
  assertEquals(1, Math.pow(null, null));
  assertEquals(NaN, Math.pow(void 0, void 0));
  assertEquals(1, Math.pow(true, false));
  assertEquals(0, Math.pow(false, true));
  assertEquals(Infinity, Math.pow(-Infinity, Infinity));
  assertEquals(0, Math.pow(-Infinity, -Infinity));
  assertEquals(1, Math.pow(0, 0));
  assertEquals(0, Math.pow(0, Infinity));
  assertEquals(NaN, Math.pow(NaN, 0.5));
  assertEquals(NaN, Math.pow(NaN, -0.5));

  // Tests from Sputnik S8.5_A13_T1.
  assertTrue(
      (1*((Math.pow(2,53))-1)*(Math.pow(2,-1074))) === 4.4501477170144023e-308);
  assertTrue(
      (1*(Math.pow(2,52))*(Math.pow(2,-1074))) === 2.2250738585072014e-308);
  assertTrue(
      (-1*(Math.pow(2,52))*(Math.pow(2,-1074))) === -2.2250738585072014e-308);
}

test();
test();
%OptimizeFunctionOnNextCall(test);
test();