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

// Test bitwise operations with booleans.

var t = 1;

function testFalseLeftHandSide() {
  var b;
  if (t) b = false;
  assertEquals(b | 1, 1);
  assertEquals(b & 1, 0);
  assertEquals(b ^ 1, 1);
  assertEquals(b << 1, 0);
  assertEquals(b >> 1, 0);
  assertEquals(b >>> 1, 0);
}

function testFalseRightHandSide() {
  if (t) b = false;
  assertEquals(1 |   b, 1);
  assertEquals(1 &   b, 0);
  assertEquals(1 ^   b, 1);
  assertEquals(1 <<  b, 1);
  assertEquals(1 >>  b, 1);
  assertEquals(1 >>> b, 1);
}

function testTrueLeftHandSide() {
  if (t) b = true;
  assertEquals(b | 1, 1);
  assertEquals(b & 1, 1);
  assertEquals(b ^ 1, 0);
  assertEquals(b << 1, 2);
  assertEquals(b >> 1, 0);
  assertEquals(b >>> 1, 0);
}

function testTrueRightHandSide() {
  if (t) b = true;
  assertEquals(1 |   b, 1);
  assertEquals(1 &   b, 1);
  assertEquals(1 ^   b, 0);
  assertEquals(1 <<  b, 2);
  assertEquals(1 >>  b, 0);
  assertEquals(1 >>> b, 0);
}

function testBothSides() {
  if (t) a = true;
  if (t) b = false;
  assertEquals(a |   b, 1);
  assertEquals(a &   b, 0);
  assertEquals(a ^   b, 1);
  assertEquals(a <<  b, 1);
  assertEquals(a >>  b, 1);
  assertEquals(a >>> b, 1);
}


testFalseLeftHandSide();
testFalseRightHandSide();
testTrueLeftHandSide();
testTrueRightHandSide();
testFalseLeftHandSide();
testFalseRightHandSide();
testTrueLeftHandSide();
testTrueRightHandSide();
testBothSides();
testBothSides();
