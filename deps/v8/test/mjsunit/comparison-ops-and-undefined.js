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

function test_helper_for_ics(func, b1, b2, b3, b4) {
  assertEquals(b1, func(.5, .5));
  assertEquals(b2, func(.5, undefined));
  assertEquals(b3, func(undefined, .5));
  assertEquals(b4, func(undefined, undefined));
}

function test_helper_for_crankshaft(func, b1, b2, b3, b4) {
  %PrepareFunctionForOptimization(func);
  assertEquals(b1, func(.5, .5));
  %OptimizeFunctionOnNextCall(func);
  assertEquals(b1, func(.5, .5));
  assertEquals(b2, func(.5, undefined));
  assertEquals(b3, func(undefined, .5));
  assertEquals(b4, func(undefined, undefined));
}

function less_1(a, b) {
  return a < b;
}

test_helper_for_ics(less_1, false, false, false, false);

function less_2(a, b) {
  return a < b;
}

test_helper_for_crankshaft(less_1, false, false, false, false);

function greater_1(a, b) {
  return a > b;
}

test_helper_for_ics(greater_1, false, false, false, false);

function greater_2(a, b) {
  return a > b;
}

test_helper_for_crankshaft(greater_1, false, false, false, false);

function less_equal_1(a, b) {
  return a <= b;
}

test_helper_for_ics(less_equal_1, true, false, false, false);

function less_equal_2(a, b) {
  return a <= b;
}

test_helper_for_crankshaft(less_equal_1, true, false, false, false);

function greater_equal_1(a, b) {
  return a >= b;
}

test_helper_for_ics(greater_equal_1, true, false, false, false);

function greater_equal_2(a, b) {
  return a >= b;
}

test_helper_for_crankshaft(greater_equal_1, true, false, false, false);

function equal_1(a, b) {
  return a == b;
}

test_helper_for_ics(equal_1, true, false, false, true);

function equal_2(a, b) {
  return a == b;
}

test_helper_for_crankshaft(equal_2, true, false, false, true);

function strict_equal_1(a, b) {
  return a === b;
}

test_helper_for_ics(strict_equal_1, true, false, false, true);

function strict_equal_2(a, b) {
  return a === b;
}

test_helper_for_crankshaft(strict_equal_2, true, false, false, true);

function not_equal_1(a, b) {
  return a != b;
}

test_helper_for_ics(not_equal_1, false, true, true, false);

function not_equal_2(a, b) {
  return a != b;
}

test_helper_for_crankshaft(not_equal_2, false, true, true, false);
