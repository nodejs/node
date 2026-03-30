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

// Flags: --max-semi-space-size=1 --allow-natives-syntax

var test_id = 0;

function testFloor(expect, input) {
  var test = new Function('n',
                          '"' + (test_id++) + '";return Math.floor(n)');
  %PrepareFunctionForOptimization(test);
  assertEquals(expect, test(input));
  assertEquals(expect, test(input));
  assertEquals(expect, test(input));
  %OptimizeFunctionOnNextCall(test);
  assertEquals(expect, test(input));

  var test_double_input = new Function(
      'n',
      '"' + (test_id++) + '";return Math.floor(+n)');
  %PrepareFunctionForOptimization(test_double_input);
  assertEquals(expect, test_double_input(input));
  assertEquals(expect, test_double_input(input));
  assertEquals(expect, test_double_input(input));
  %OptimizeFunctionOnNextCall(test_double_input);
  assertEquals(expect, test_double_input(input));

  var test_double_output = new Function(
      'n',
      '"' + (test_id++) + '";return Math.floor(n) + -0.0');
  %PrepareFunctionForOptimization(test_double_output);
  assertEquals(expect, test_double_output(input));
  assertEquals(expect, test_double_output(input));
  assertEquals(expect, test_double_output(input));
  %OptimizeFunctionOnNextCall(test_double_output);
  assertEquals(expect, test_double_output(input));

  var test_via_ceil = new Function(
      'n',
      '"' + (test_id++) + '";return -Math.ceil(-n)');
  %PrepareFunctionForOptimization(test_via_ceil);
  assertEquals(expect, test_via_ceil(input));
  assertEquals(expect, test_via_ceil(input));
  assertEquals(expect, test_via_ceil(input));
  %OptimizeFunctionOnNextCall(test_via_ceil);
  assertEquals(expect, test_via_ceil(input));

  if (input >= 0) {
    var test_via_trunc = new Function(
        'n',
        '"' + (test_id++) + '";return Math.trunc(n)');
    %PrepareFunctionForOptimization(test_via_trunc);
    assertEquals(expect, test_via_trunc(input));
    assertEquals(expect, test_via_trunc(input));
    assertEquals(expect, test_via_trunc(input));
    %OptimizeFunctionOnNextCall(test_via_trunc);
    assertEquals(expect, test_via_trunc(input));
  }
}

function zero() {
  var x = 0.5;
  return (function() { return x - 0.5; })();
}

function test() {
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
  testFloor(0, 1.0 - Number.EPSILON);
  testFloor(-1, -0.1);
  testFloor(-1, -0.49999999999999994);
  testFloor(-1, -0.5);
  testFloor(-1, -0.7);
  testFloor(1, 1);
  testFloor(1, 1.1);
  testFloor(1, 1.0 + Number.EPSILON);
  testFloor(1, 1.5);
  testFloor(1, 1.7);
  testFloor(-1, -1);
  testFloor(-1, -1 + Number.EPSILON);
  testFloor(-2, -1 - Number.EPSILON);
  testFloor(-2, -1.1);
  testFloor(-2, -1.5);
  testFloor(-2, -1.7);

  testFloor(0, Number.MIN_VALUE);
  testFloor(-1, -Number.MIN_VALUE);
  testFloor(Number.MAX_VALUE, Number.MAX_VALUE);
  testFloor(-Number.MAX_VALUE, -Number.MAX_VALUE);
  testFloor(Infinity, Infinity);
  testFloor(-Infinity, -Infinity);
}


// Test in a loop to cover the custom IC and GC-related issues.
for (var i = 0; i < 10; i++) {
  test();
  new Array(i * 10000);
}
