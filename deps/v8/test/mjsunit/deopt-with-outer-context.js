// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function outer(y) {
  function inner() {
    var x = 10;
    (function() {
       // Access x from inner function to force it to be context allocated.
       x = 20;
       %DeoptimizeFunction(inner);
    })();
    // Variable y should be read from the outer context.
    return y;
  };
  %OptimizeFunctionOnNextCall(inner);
  return inner();
}

assertEquals(30, outer(30));
