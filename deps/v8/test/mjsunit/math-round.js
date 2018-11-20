// Copyright 2011 the V8 project authors. All rights reserved.
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

var test_id = 0;
function testRound(expect, input) {
  // Make source code different on each invocation to make
  // sure it gets optimized each time.
  var doRound = new Function('input',
                             '"' + (test_id++) + '";return Math.round(input)');
  assertEquals(expect, doRound(input));
  assertEquals(expect, doRound(input));
  assertEquals(expect, doRound(input));
  %OptimizeFunctionOnNextCall(doRound);
  assertEquals(expect, doRound(input));

  // Force the Math.round() representation to double to exercise the associated
  // optimized code.
  var doRoundToDouble = new Function('input',
                                     '"' + (test_id++) + '";return Math.round(input) + -0.0');
  assertEquals(expect, doRoundToDouble(input));
  assertEquals(expect, doRoundToDouble(input));
  assertEquals(expect, doRoundToDouble(input));
  %OptimizeFunctionOnNextCall(doRoundToDouble);
  assertEquals(expect, doRoundToDouble(input));
}

testRound(0, 0);
testRound(-0, -0);
testRound(Infinity, Infinity);
testRound(-Infinity, -Infinity);
testRound(NaN, NaN);

// Regression test for a bug where a negative zero coming from Math.round
// was not properly handled by other operations.
function roundsum(i, n) {
  var ret = Math.round(n);
  while (--i > 0) {
    ret += Math.round(n);
  }
  return ret;
}
assertEquals(-0, roundsum(1, -0));
%OptimizeFunctionOnNextCall(roundsum);
// The optimized function will deopt.  Run it with enough iterations to try
// to optimize via OSR (triggering the bug).
assertEquals(-0, roundsum(100000, -0));

testRound(1, 0.5);
testRound(1, 0.7);
testRound(1, 1);
testRound(1, 1.1);
testRound(1, 1.49999);
testRound(-0, -0.5);
testRound(-1, -0.5000000000000001);
testRound(-1, -0.7);
testRound(-1, -1);
testRound(-1, -1.1);
testRound(-1, -1.49999);
testRound(-1, -1.5);

testRound(9007199254740990, 9007199254740990);
testRound(9007199254740991, 9007199254740991);
testRound(-9007199254740990, -9007199254740990);
testRound(-9007199254740991, -9007199254740991);
testRound(Number.MAX_VALUE, Number.MAX_VALUE);
testRound(-Number.MAX_VALUE, -Number.MAX_VALUE);
testRound(Number.MAX_SAFE_INTEGER, Number.MAX_SAFE_INTEGER);
testRound(Number.MAX_SAFE_INTEGER + 1, Number.MAX_SAFE_INTEGER + 1);
testRound(Number.MAX_SAFE_INTEGER + 2, Number.MAX_SAFE_INTEGER + 2);
testRound(Number.MAX_SAFE_INTEGER + 3, Number.MAX_SAFE_INTEGER + 3);
testRound(Number.MAX_SAFE_INTEGER + 4, Number.MAX_SAFE_INTEGER + 4);
testRound(Number.MIN_SAFE_INTEGER, Number.MIN_SAFE_INTEGER);
testRound(Number.MIN_SAFE_INTEGER - 1, Number.MIN_SAFE_INTEGER - 1);
testRound(Number.MIN_SAFE_INTEGER - 2, Number.MIN_SAFE_INTEGER - 2);
testRound(Number.MIN_SAFE_INTEGER - 3, Number.MIN_SAFE_INTEGER - 3);

testRound(536870911, 536870910.5);
testRound(536870911, 536870911);
testRound(536870911, 536870911.4);
testRound(536870912, 536870911.5);
testRound(536870912, 536870912);
testRound(536870912, 536870912.4);
testRound(536870913, 536870912.5);
testRound(536870913, 536870913);
testRound(536870913, 536870913.4);
testRound(1073741823, 1073741822.5);
testRound(1073741823, 1073741823);
testRound(1073741823, 1073741823.4);
testRound(1073741824, 1073741823.5);
testRound(1073741824, 1073741824);
testRound(1073741824, 1073741824.4);
testRound(1073741825, 1073741824.5);
testRound(2147483647, 2147483646.5);
testRound(2147483647, 2147483647);
testRound(2147483647, 2147483647.4);
testRound(2147483648, 2147483647.5);
testRound(2147483648, 2147483648);
testRound(2147483648, 2147483648.4);
testRound(2147483649, 2147483648.5);

// Tests based on WebKit LayoutTests

testRound(0, 0.4);
testRound(-0, -0.4);
testRound(-0, -0.5);
testRound(1, 0.6);
testRound(-1, -0.6);
testRound(2, 1.5);
testRound(2, 1.6);
testRound(-2, -1.6);
testRound(8640000000000000, 8640000000000000);
testRound(8640000000000001, 8640000000000001);
testRound(8640000000000002, 8640000000000002);
testRound(9007199254740990, 9007199254740990);
testRound(9007199254740991, 9007199254740991);
testRound(1.7976931348623157e+308, 1.7976931348623157e+308);
testRound(-8640000000000000, -8640000000000000);
testRound(-8640000000000001, -8640000000000001);
testRound(-8640000000000002, -8640000000000002);
testRound(-9007199254740990, -9007199254740990);
testRound(-9007199254740991, -9007199254740991);
testRound(-1.7976931348623157e+308, -1.7976931348623157e+308);
testRound(Infinity, Infinity);
testRound(-Infinity, -Infinity);

  // Some special double number cases.
var ulp = Math.pow(2, -1022 - 52);
var max_denormal = (Math.pow(2, 52) - 1) * ulp;
var min_normal = Math.pow(2, -1022);
var max_fraction = Math.pow(2, 52) - 0.5;
var min_nonfraction = Math.pow(2, 52);
var max_non_infinite = Number.MAX_VALUE;

var max_smi31 = Math.pow(2,30) - 1;
var min_smi31 = -Math.pow(2,30);
var max_smi32 = Math.pow(2,31) - 1;
var min_smi32 = -Math.pow(2,31);

testRound(0, ulp);
testRound(0, max_denormal);
testRound(0, min_normal);
testRound(0, 0.49999999999999994);
testRound(1, 0.5);
testRound(Math.pow(2,52), max_fraction);
testRound(min_nonfraction, min_nonfraction);
testRound(max_non_infinite, max_non_infinite);

testRound(max_smi31, max_smi31 - 0.5);
testRound(max_smi31 + 1, max_smi31 + 0.5);
testRound(max_smi32, max_smi32 - 0.5);
testRound(max_smi32 + 1, max_smi32 + 0.5);

testRound(-0, -ulp);
testRound(-0, -max_denormal);
testRound(-0, -min_normal);
testRound(-0, -0.49999999999999994);
testRound(-0, -0.5);
testRound(-Math.pow(2,52)+1, -max_fraction);
testRound(-min_nonfraction, -min_nonfraction);
testRound(-max_non_infinite, -max_non_infinite);

testRound(min_smi31, min_smi31 - 0.5);
testRound(min_smi31 + 1, min_smi31 + 0.5);
testRound(min_smi32, min_smi32 - 0.5);
testRound(min_smi32 + 1, min_smi32 + 0.5);
