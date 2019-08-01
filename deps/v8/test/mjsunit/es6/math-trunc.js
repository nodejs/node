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

// Flags: --allow-natives-syntax

var test_id = 0;

function testTrunc(expected, input) {
  var test = new Function('n',
                          '"' + (test_id++) + '";return Math.trunc(n)');
  assertEquals(expected, test(input));
  assertEquals(expected, test(input));
  assertEquals(expected, test(input));
  %PrepareFunctionForOptimization(test);
  %OptimizeFunctionOnNextCall(test);
  assertEquals(expected, test(input));

  var test_double_input = new Function(
      'n',
      '"' + (test_id++) + '";return Math.trunc(+n)');
  %PrepareFunctionForOptimization(test_double_input);
  assertEquals(expected, test_double_input(input));
  assertEquals(expected, test_double_input(input));
  assertEquals(expected, test_double_input(input));
  %OptimizeFunctionOnNextCall(test_double_input);
  assertEquals(expected, test_double_input(input));

  var test_double_output = new Function(
      'n',
      '"' + (test_id++) + '";return Math.trunc(n) + -0.0');
  %PrepareFunctionForOptimization(test_double_output);
  assertEquals(expected, test_double_output(input));
  assertEquals(expected, test_double_output(input));
  assertEquals(expected, test_double_output(input));
  %OptimizeFunctionOnNextCall(test_double_output);
  assertEquals(expected, test_double_output(input));
}

function test() {
  // Ensure that a negative zero coming from Math.trunc is properly handled
  // by other operations.
  function itrunc(x) {
    return 1 / Math.trunc(x);
  }
  %PrepareFunctionForOptimization(itrunc);
  assertEquals(Infinity, itrunc(0));
  assertEquals(-Infinity, itrunc(-0));
  assertEquals(Infinity, itrunc(Math.PI / 4));
  assertEquals(-Infinity, itrunc(-Math.sqrt(2) / 2));
  assertEquals(-Infinity, itrunc({valueOf: function() { return "-0.1"; }}));
  %OptimizeFunctionOnNextCall(itrunc);

  testTrunc(100, 100);
  testTrunc(-199, -199);
  testTrunc(100, 100.1);
  testTrunc(4503599627370495.0, 4503599627370495.0);
  testTrunc(4503599627370496.0, 4503599627370496.0);
  testTrunc(-4503599627370495.0, -4503599627370495.0);
  testTrunc(-4503599627370496.0, -4503599627370496.0);
  testTrunc(9007199254740991.0, 9007199254740991.0);
  testTrunc(-9007199254740991.0, -9007199254740991.0);
  testTrunc(0, []);
  testTrunc(1, [1]);
  testTrunc(-100, [-100.1]);
  testTrunc(-100, {toString: function() { return "-100.3"; }});
  testTrunc(10, {toString: function() { return 10.1; }});
  testTrunc(-1, {valueOf: function() { return -1.1; }});
  testTrunc(-Infinity, -Infinity);
  testTrunc(Infinity, Infinity);
  testTrunc(-Infinity, "-Infinity");
  testTrunc(Infinity, "Infinity");

  assertTrue(isNaN(Math.trunc("abc")));
  assertTrue(isNaN(Math.trunc({})));
  assertTrue(isNaN(Math.trunc([1, 1])));
}

// Test in a loop to cover the custom IC and GC-related issues.
for (var i = 0; i < 10; i++) {
  test();
  new Array(i * 10000);
}
