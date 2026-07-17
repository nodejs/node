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

// Test materialization of the arguments object when deoptimizing a
// strict mode closure after modifying an argument.

(function () {
  var forceDeopt = 0;
  function inner(x) {
    "use strict";
    x = 2;
    // Do not remove this %DebugPrint as it makes sure the deopt happens
    // after the assignment and is not hoisted above the assignment.
    %DebugPrint(arguments[0]);
    forceDeopt + 1;
    return arguments[0];
  };
  %PrepareFunctionForOptimization(inner);
  assertEquals(1, inner(1));
  assertEquals(1, inner(1));
  %OptimizeFunctionOnNextCall(inner);
  assertEquals(1, inner(1));
  forceDeopt = "not a number";
  assertEquals(1, inner(1));
})();


// Test materialization of the arguments object when deoptimizing an
// inlined strict mode closure after modifying an argument.

(function () {
  var forceDeopt = 0;
  function inner(x) {
    "use strict";
    x = 2;
    // Do not remove this %DebugPrint as it makes sure the deopt happens
    // after the assignment and is not hoisted above the assignment.
    %DebugPrint(arguments[0]);
    forceDeopt + 1;
    return arguments[0];
  }

  function outer(x) {
    return inner(x);
  };
  %PrepareFunctionForOptimization(outer);
  assertEquals(1, outer(1));
  assertEquals(1, outer(1));
  %OptimizeFunctionOnNextCall(outer);
  assertEquals(1, outer(1));
  forceDeopt = "not a number";
  assertEquals(1, outer(1));
})();


// Test materialization of the multiple arguments objects when
// deoptimizing several inlined closure after modifying an argument.

(function () {
  var forceDeopt = 0;
  function inner(x, y, z) {
    "use strict";
    x = 3;
    // Do not remove this %DebugPrint as it makes sure the deopt happens
    // after the assignment and is not hoisted above the assignment.
    %DebugPrint(arguments[0]);
    forceDeopt + 1;
    return arguments[0];
  }

  function middle(x) {
    "use strict";
    x = 2;
    return inner(10 * x, 20 * x, 30 * x) + arguments[0];
  }

  function outer(x) {
    return middle(x);
  };
  %PrepareFunctionForOptimization(outer);
  assertEquals(21, outer(1));
  assertEquals(21, outer(1));
  %OptimizeFunctionOnNextCall(outer);
  assertEquals(21, outer(1));
  forceDeopt = "not a number";
  assertEquals(21, outer(1));
})();
