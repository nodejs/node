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

// Flags: --max-semi-space-size=1 --allow-natives-syntax

// Test inlining of Math.floor when assigned to a local.
var test_id = 0;

function testFloor(expect, input) {
  var test = new Function('n',
                          '"' + (test_id++) +
                          '";var f = Math.floor; return f(n)');
  %PrepareFunctionForOptimization(test);
  assertEquals(expect, test(input));
  assertEquals(expect, test(input));
  assertEquals(expect, test(input));
  %OptimizeFunctionOnNextCall(test);
  assertEquals(expect, test(input));
}

function zero() {
  var x = 0.5;
  return (function() { return x - 0.5; })();
}

function test() {
  testFloor(0, 0);
  testFloor(0, zero());
  testFloor(-0, -0);
  testFloor(Infinity, Infinity);
  testFloor(-Infinity, -Infinity);
  testFloor(NaN, NaN);

  // Ensure that a negative zero coming from Math.floor is properly handled
  // by other operations.
  function ifloor(x) {
    return 1 / Math.floor(x);
  }
  %PrepareFunctionForOptimization(ifloor);
  assertEquals(-Infinity, ifloor(-0));
  assertEquals(-Infinity, ifloor(-0));
  assertEquals(-Infinity, ifloor(-0));
  %OptimizeFunctionOnNextCall(ifloor);
  assertEquals(-Infinity, ifloor(-0));

  testFloor(0, 0.1);
  testFloor(0, 0.49999999999999994);
  testFloor(0, 0.5);
  testFloor(0, 0.7);
  testFloor(-1, -0.1);
  testFloor(-1, -0.49999999999999994);
  testFloor(-1, -0.5);
  testFloor(-1, -0.7);
  testFloor(1, 1);
  testFloor(1, 1.1);
  testFloor(1, 1.5);
  testFloor(1, 1.7);
  testFloor(-1, -1);
  testFloor(-2, -1.1);
  testFloor(-2, -1.5);
  testFloor(-2, -1.7);

  testFloor(0, Number.MIN_VALUE);
  testFloor(-1, -Number.MIN_VALUE);
  testFloor(Number.MAX_VALUE, Number.MAX_VALUE);
  testFloor(-Number.MAX_VALUE, -Number.MAX_VALUE);
  testFloor(Infinity, Infinity);
  testFloor(-Infinity, -Infinity);

  // 2^30 is a smi boundary.
  var two_30 = 1 << 30;

  testFloor(two_30, two_30);
  testFloor(two_30, two_30 + 0.1);
  testFloor(two_30, two_30 + 0.5);
  testFloor(two_30, two_30 + 0.7);

  testFloor(two_30 - 1, two_30 - 1);
  testFloor(two_30 - 1, two_30 - 1 + 0.1);
  testFloor(two_30 - 1, two_30 - 1 + 0.5);
  testFloor(two_30 - 1, two_30 - 1 + 0.7);

  testFloor(-two_30, -two_30);
  testFloor(-two_30, -two_30 + 0.1);
  testFloor(-two_30, -two_30 + 0.5);
  testFloor(-two_30, -two_30 + 0.7);

  testFloor(-two_30 + 1, -two_30 + 1);
  testFloor(-two_30 + 1, -two_30 + 1 + 0.1);
  testFloor(-two_30 + 1, -two_30 + 1 + 0.5);
  testFloor(-two_30 + 1, -two_30 + 1 + 0.7);

  // 2^52 is a precision boundary.
  var two_52 = (1 << 30) * (1 << 22);

  testFloor(two_52, two_52);
  testFloor(two_52, two_52 + 0.1);
  assertEquals(two_52, two_52 + 0.5);
  testFloor(two_52, two_52 + 0.5);
  assertEquals(two_52 + 1, two_52 + 0.7);
  testFloor(two_52 + 1, two_52 + 0.7);

  testFloor(two_52 - 1, two_52 - 1);
  testFloor(two_52 - 1, two_52 - 1 + 0.1);
  testFloor(two_52 - 1, two_52 - 1 + 0.5);
  testFloor(two_52 - 1, two_52 - 1 + 0.7);

  testFloor(-two_52, -two_52);
  testFloor(-two_52, -two_52 + 0.1);
  testFloor(-two_52, -two_52 + 0.5);
  testFloor(-two_52, -two_52 + 0.7);

  testFloor(-two_52 + 1, -two_52 + 1);
  testFloor(-two_52 + 1, -two_52 + 1 + 0.1);
  testFloor(-two_52 + 1, -two_52 + 1 + 0.5);
  testFloor(-two_52 + 1, -two_52 + 1 + 0.7);
}


// Test in a loop to cover the custom IC and GC-related issues.
for (var i = 0; i < 10; i++) {
  test();
  new Array(i * 10000);
}


// Regression test for a bug where a negative zero coming from Math.floor
// was not properly handled by other operations.
function floorsum(i, n) {
  var ret = Math.floor(n);
  while (--i > 0) {
    ret += Math.floor(n);
  }
  return ret;
}
%PrepareFunctionForOptimization(floorsum);
assertEquals(-0, floorsum(1, -0));
%OptimizeFunctionOnNextCall(floorsum);
// The optimized function will deopt.  Run it with enough iterations to try
// to optimize via OSR (triggering the bug).
assertEquals(-0, floorsum(100000, -0));
