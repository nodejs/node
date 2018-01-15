// Copyright 2014 the V8 project authors. All rights reserved.
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

// Flags: --allow-natives-syntax --opt --noalways-opt

function check(func, input, expected) {
  func(-1);
  func(-1);
  %OptimizeFunctionOnNextCall(func);
  assertEquals(expected, func(input));
  assertOptimized(func);
}

function mul_by_neg_1(a) { return a * -1; }
function mul_by_0(a) {  return a * 0; }
function mul_by_1(a) { return a * 1; }
function mul_by_2(a) { return a * 2; }

check(mul_by_neg_1, 2, -2);
check(mul_by_0, 2, 0);
check(mul_by_1, 2, 2);
check(mul_by_2, 2, 4);

function limit_range(a) {
  // Limit the range of 'a' to enable no-overflow optimizations.
  return Math.max(Math.min(a | 0, 10), -10);
}

function mul_by_neg_127(a) { return limit_range(a) * -127; }
function mul_by_neg_128(a) { return limit_range(a) * -128; }
function mul_by_neg_129(a) { return limit_range(a) * -129; }
function mul_by_1023(a) { return limit_range(a) * 1023; }
function mul_by_1024(a) { return limit_range(a) * 1024; }
function mul_by_1025(a) { return limit_range(a) * 1025; }

check(mul_by_neg_127, 2, -254);
check(mul_by_neg_128, 2, -256);
check(mul_by_neg_129, 2, -258);
check(mul_by_1023, 2, 2046);
check(mul_by_1024, 2, 2048);
check(mul_by_1025, 2, 2050);

// Deopt on minus zero.
assertEquals(-0, mul_by_neg_128(0));
assertUnoptimized(mul_by_neg_128);
assertEquals(-0, mul_by_2(-0));
assertUnoptimized(mul_by_2);

// Deopt on overflow.

// 2^30 is a smi boundary on arm and ia32.
var two_30 = 1 << 30;
// 2^31 is a smi boundary on arm64 and x64.
var two_31 = 2 * two_30;

// TODO(rmcilroy): replace after r16361 with:  if (%IsValidSmi(two_31)) {
if (true) {
  assertEquals(two_31, mul_by_neg_1(-two_31));
  assertUnoptimized(mul_by_neg_1);
} else {
  assertEquals(two_30, mul_by_neg_1(-two_30));
  assertUnoptimized(mul_by_neg_1);
}
