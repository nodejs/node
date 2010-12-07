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

function MaxLT(x, y) {
  if (x < y) return y;
  return x;
}

function MaxLE(x, y) {
  if (x <= y) return y;
  return x;
}

function MaxGE(x, y) {
  if (x >= y) return x;
  return y;
}

function MaxGT(x, y) {
  if (x > y) return x;
  return y;
}


// First test primitive values.
function TestPrimitive(max, x, y) {
  assertEquals(max, MaxLT(x, y), "MaxLT - primitive");
  assertEquals(max, MaxLE(x, y), "MaxLE - primitive");
  assertEquals(max, MaxGE(x, y), "MaxGE - primitive");
  assertEquals(max, MaxGT(x, y), "MaxGT - primitive");
}

TestPrimitive(1, 0, 1);
TestPrimitive(1, 1, 0);
TestPrimitive(4, 3, 4);
TestPrimitive(4, 4, 3);
TestPrimitive(0, -1, 0);
TestPrimitive(0, 0, -1)
TestPrimitive(-2, -2, -3);
TestPrimitive(-2, -3, -2);

TestPrimitive(1, 0.1, 1);
TestPrimitive(1, 1, 0.1);
TestPrimitive(4, 3.1, 4);
TestPrimitive(4, 4, 3.1);
TestPrimitive(0, -1.1, 0);
TestPrimitive(0, 0, -1.1)
TestPrimitive(-2, -2, -3.1);
TestPrimitive(-2, -3.1, -2);


// Test non-primitive values and watch for valueOf call order.
function TestNonPrimitive(order, f) {
  var result = "";
  var x = { valueOf: function() { result += "x"; } };
  var y = { valueOf: function() { result += "y"; } };
  f(x, y);
  assertEquals(order, result);
}

TestNonPrimitive("xy", MaxLT);
TestNonPrimitive("yx", MaxLE);
TestNonPrimitive("xy", MaxGE);
TestNonPrimitive("yx", MaxGT);

// Test compare in case of aliased registers.
function CmpX(x) { if (x == x) return 42; }
assertEquals(42, CmpX(0));

function CmpXY(x) { var y = x; if (x == y) return 42; }
assertEquals(42, CmpXY(0));


// Test compare against null.
function CmpNullValue(x) { return x == null; }
assertEquals(false, CmpNullValue(42));

function CmpNullTest(x) { if (x == null) return 42; return 0; }
assertEquals(42, CmpNullTest(null));

var g1 = 0;
function CmpNullEffect() { (g1 = 42) == null; }
CmpNullEffect();
assertEquals(42, g1);
