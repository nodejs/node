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

// Flags: --max-new-space-size=256 --allow-natives-syntax

var test_id = 0;

function testFloor(expect, input) {
  var test = new Function('n',
                          '"' + (test_id++) + '";return Math.floor(n)');
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
}


// Test in a loop to cover the custom IC and GC-related issues.
for (var i = 0; i < 100; i++) {
  test();
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
assertEquals(-0, floorsum(1, -0));
%OptimizeFunctionOnNextCall(floorsum);
// The optimized function will deopt.  Run it with enough iterations to try
// to optimize via OSR (triggering the bug).
assertEquals(-0, floorsum(100000, -0));
