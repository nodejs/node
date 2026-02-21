// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function foo() {
  const the_array = [0, 0];
  function callback(element, index, array) {
    // Right-trim the array on each iteration, so that the second iteration
    // fails.
    array.shift();
    assertEquals(0, element);
  }
  the_array.forEach(callback);
}

%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
