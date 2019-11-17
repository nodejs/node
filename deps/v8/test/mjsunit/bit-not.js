// Copyright 2009 the V8 project authors. All rights reserved.
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

function testBitNot(x, name) {
  // The VM constant folds so we use that to check the result.
  var expected = eval("~(" + x + ")");
  var actual = ~x;
  assertEquals(expected, actual, "x: " + name);

  // Test the path where we can overwrite the result. Use -
  // to avoid concatenating strings.
  expected = eval("~(" + x + " - 0.01)");
  actual = ~(x - 0.01);
  assertEquals(expected, actual, "x - 0.01: " + name);
}


testBitNot(0, 0);
testBitNot(1, 1);
testBitNot(-1, 1);
testBitNot(100, 100);
testBitNot(0x40000000, "0x40000000");
testBitNot(0x7fffffff, "0x7fffffff");
testBitNot(0x80000000, "0x80000000");

testBitNot(2.2, 2.2);
testBitNot(-2.3, -2.3);
testBitNot(Infinity, "Infinity");
testBitNot(NaN, "NaN");
testBitNot(-Infinity, "-Infinity");
testBitNot(0x40000000 + 0.12345, "float1");
testBitNot(0x40000000 - 0.12345, "float2");
testBitNot(0x7fffffff + 0.12345, "float3");
testBitNot(0x7fffffff - 0.12345, "float4");
testBitNot(0x80000000 + 0.12345, "float5");
testBitNot(0x80000000 - 0.12345, "float6");

testBitNot("0", "string0");
testBitNot("2.3", "string2.3");
testBitNot("-9.4", "string-9.4");
