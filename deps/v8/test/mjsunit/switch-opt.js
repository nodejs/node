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

(function() {
  var result = [];
  var x = 0;

  function branch(b) {
    if (b == "deopt") {
      %DeoptimizeFunction(f);
      return "c";
    }

    return b ? "a" : "b";
  }

  function f(label, b1, b2, b3) {
    switch (label) {
      case "string":
        result.push(1);
        break;
      case branch(b1) + branch(b2):
        result.push(2);
        break;
      case 10:
        result.push(3);
        break;
      default:
        branch(b3);
        result.push(4);
        break;
      case x++:
        branch(b3);
        result.push(5);
        break;
    }
  }
  %PrepareFunctionForOptimization(f);

  function assertResult(r, label, b1, b2, b3) {
    f(label, b1, b2, b3);
    assertEquals(result, r);
    result = [];
  }

  // Warmup.
  assertResult([2], "aa", true, true);
  assertResult([2], "ab", true, false);
  assertResult([2], "ba", false, true);
  assertResult([2], "bb", false, false);
  assertEquals(0, x);
  assertResult([4], "other");
  assertEquals(1, x);
  assertResult([5], 1, true, true);
  assertResult([4], 1, true, true);
  assertResult([5], 3, true, true);
  assertResult([4], 3, true, true);
  assertResult([5], 5, true, true);
  assertResult([4], 5, true, true);
  assertEquals(7, x);

  // Test regular behavior.
  %OptimizeFunctionOnNextCall(f);
  assertResult([2], "aa", true, true);
  assertResult([1], "string");
  assertResult([4], "other");
  assertEquals(8, x);
  assertResult([5], 8);
  assertEquals(9, x);

  // Test deopt at the beginning of the case label evaluation.
  %PrepareFunctionForOptimization(f);
  assertResult([2], "ca", "deopt", true);
  %OptimizeFunctionOnNextCall(f);
  assertResult([4], "ca", "deopt", false);
  assertEquals(10, x);
  %PrepareFunctionForOptimization(f);
  %OptimizeFunctionOnNextCall(f);

  // Test deopt in the middle of the case label evaluation.
  assertResult([2], "ac", true, "deopt");
  %PrepareFunctionForOptimization(f);
  %OptimizeFunctionOnNextCall(f);
  assertResult([4], "ac", false, "deopt");
  assertEquals(11, x);

  // Test deopt in the default case.
  %PrepareFunctionForOptimization(f);
  %OptimizeFunctionOnNextCall(f);
  print("here");
  assertResult([4], 10000, false, false, "deopt");
  assertEquals(12, x);

  // Test deopt in the default case.
  %PrepareFunctionForOptimization(f);
  %OptimizeFunctionOnNextCall(f);
  assertResult([4], 10000, false, false, "deopt");
  assertEquals(13, x);

  // Test deopt in x++ case.
  %PrepareFunctionForOptimization(f);
  %OptimizeFunctionOnNextCall(f);
  assertResult([5], 13, false, false, "deopt");
  assertEquals(14, x);
})();


(function() {
  var result = [];
  var x = 0;

  function branch(b) {
    if (b == "deopt") {
      %DeoptimizeFunction(f);
      return "c";
    }

    return b ? "a" : "b";
  }

  function f(label, b1, b2, b3) {
    switch (label) {
      case "string":
        result.push(1);
        break;
      case branch(b1) + branch(b2):
        result.push(2);
        // Fall through.
      case 10:
        result.push(3);
        break;
      default:
        branch(b3);
        result.push(4);
        // Fall through.
      case x++:
        branch(b3);
        result.push(5);
        break;
    }
  }
  %PrepareFunctionForOptimization(f);

  function assertResult(r, label, b1, b2, b3) {
    f(label, b1, b2, b3);
    assertEquals(r, result);
    result = [];
  }

  // Warmup.
  assertResult([2,3], "aa", true, true);
  assertResult([2,3], "ab", true, false);
  assertResult([2,3], "ba", false, true);
  assertResult([2,3], "bb", false, false);
  assertEquals(0, x);
  assertResult([4,5], "other");
  assertEquals(1, x);
  assertResult([5], 1, true, true);
  assertResult([4,5], 1, true, true);
  assertResult([5], 3, true, true);
  assertResult([4,5], 3, true, true);
  assertResult([5], 5, true, true);
  assertResult([4,5], 5, true, true);
  assertEquals(7, x);

  // Test regular behavior.
  %OptimizeFunctionOnNextCall(f);
  assertResult([2,3], "aa", true, true);
  assertResult([1], "string");
  assertResult([4,5], "other");
  assertEquals(8, x);
  assertResult([5], 8);
  assertEquals(9, x);

  // Test deopt at the beginning of the case label evaluation.
  %PrepareFunctionForOptimization(f);
  assertResult([2,3], "ca", "deopt", true);
  %OptimizeFunctionOnNextCall(f);
  assertResult([4,5], "ca", "deopt", false);
  assertEquals(10, x);
  %PrepareFunctionForOptimization(f);
  %OptimizeFunctionOnNextCall(f);

  // Test deopt in the middle of the case label evaluation.
  assertResult([2,3], "ac", true, "deopt");
  %PrepareFunctionForOptimization(f);
  %OptimizeFunctionOnNextCall(f);
  assertResult([4,5], "ac", false, "deopt");
  assertEquals(11, x);

  // Test deopt in the default case.
  %PrepareFunctionForOptimization(f);
  %OptimizeFunctionOnNextCall(f);
  print("here");
  assertResult([4,5], 10000, false, false, "deopt");
  assertEquals(12, x);

  // Test deopt in the default case.
  %PrepareFunctionForOptimization(f);
  %OptimizeFunctionOnNextCall(f);
  assertResult([4,5], 10000, false, false, "deopt");
  assertEquals(13, x);

  // Test deopt in x++ case.
  %PrepareFunctionForOptimization(f);
  %OptimizeFunctionOnNextCall(f);
  assertResult([5], 13, false, false, "deopt");
  assertEquals(14, x);
})();
