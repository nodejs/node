// Copyright 2012 the V8 project authors. All rights reserved.
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

// Test that we can inline functions containing materialized literals.

function a2(b, c) {
  return [b, c, b + c];
}

function a1(a, b, c) {
  return [a, a2(b, c)];
}

function TestArrayLiteral(a, b, c) {
  var expected = [a, [b, c, b + c]];
  var result = a1(a, b, c);
  assertEquals(expected, result, "TestArrayLiteral");
}

TestArrayLiteral(1, 2, 3);
TestArrayLiteral(1, 2, 3);
%OptimizeFunctionOnNextCall(TestArrayLiteral);
TestArrayLiteral(1, 2, 3);
TestArrayLiteral('a', 'b', 'c');

function o2(b, c) {
  return { 'b':b, 'c':c, 'y':b + c };
}

function o1(a, b, c) {
  return { 'a':a, 'x':o2(b, c) };
}

function TestObjectLiteral(a, b, c) {
  var expected = { 'a':a, 'x':{ 'b':b, 'c':c, 'y':b + c } };
  var result = o1(a, b, c);
  assertEquals(expected, result, "TestObjectLiteral");
}

TestObjectLiteral(1, 2, 3);
TestObjectLiteral(1, 2, 3);
%OptimizeFunctionOnNextCall(TestObjectLiteral);
TestObjectLiteral(1, 2, 3);
TestObjectLiteral('a', 'b', 'c');

function r2(s, x, y) {
  return s.replace(/a/, x + y);
}

function r1(s, x, y) {
  return r2(s, x, y).replace(/b/, y + x);
}

function TestRegExpLiteral(s, x, y, expected) {
  var result = r1(s, x, y);
  assertEquals(expected, result, "TestRegExpLiteral");
}

TestRegExpLiteral("a-", "reg", "exp", "regexp-");
TestRegExpLiteral("-b", "reg", "exp", "-expreg");
%OptimizeFunctionOnNextCall(TestRegExpLiteral);
TestRegExpLiteral("ab", "reg", "exp", "regexpexpreg");
TestRegExpLiteral("ab", 12345, 54321, "6666666666");
