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

function add(a, b) {
  return a + b;
}

function testToString(a, b) {
  assertEquals(a, b.toString());
  assertEquals(a, "" + b);
  assertEquals(a, add("", b));
  assertEquals("yes" + a, add("yes", b));
}

testToString("NaN", (NaN));
testToString("Infinity", (1/0));
testToString("-Infinity", (-1/0));
testToString("0", (0));
testToString("9", (9));
testToString("90", (90));
testToString("90.12", (90.12));
testToString("0.1", (0.1));
testToString("0.01", (0.01));
testToString("0.0123", (0.0123));
testToString("111111111111111110000", (111111111111111111111));
testToString("1.1111111111111111e+21", (1111111111111111111111));
testToString("1.1111111111111111e+22", (11111111111111111111111));
testToString("0.00001", (0.00001));
testToString("0.000001", (0.000001));
testToString("1e-7", (0.0000001));
testToString("1.2e-7", (0.00000012));
testToString("1.23e-7", (0.000000123));
testToString("1e-8", (0.00000001));
testToString("1.2e-8", (0.000000012));
testToString("1.23e-8", (0.0000000123));

testToString("0", (-0));
testToString("-9", (-9));
testToString("-90", (-90));
testToString("-90.12", (-90.12));
testToString("-0.1", (-0.1));
testToString("-0.01", (-0.01));
testToString("-0.0123", (-0.0123));
testToString("-111111111111111110000", (-111111111111111111111));
testToString("-1.1111111111111111e+21", (-1111111111111111111111));
testToString("-1.1111111111111111e+22", (-11111111111111111111111));
testToString("-0.00001", (-0.00001));
testToString("-0.000001", (-0.000001));
testToString("-1e-7", (-0.0000001));
testToString("-1.2e-7", (-0.00000012));
testToString("-1.23e-7", (-0.000000123));
testToString("-1e-8", (-0.00000001));
testToString("-1.2e-8", (-0.000000012));
testToString("-1.23e-8", (-0.0000000123));

testToString("1000", (1000));
testToString("0.00001", (0.00001));
testToString("1000000000000000100", (1000000000000000128));
testToString("1e+21", (1000000000000000012800));
testToString("-1e+21", (-1000000000000000012800));
testToString("1e-7", (0.0000001));
testToString("-1e-7", (-0.0000001));
testToString("1.0000000000000001e+21", (1000000000000000128000));
testToString("0.000001", (0.000001));
testToString("1e-7", (0.0000001));
