// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function bar(a) {
  function in_bar(a, b) {
    a % b; // This emit a generic mod that can throw.
    return a;
  }
  %PrepareFunctionForOptimization(in_bar);
  in_bar({});
  if (a) {
    throw_before_this_function_is_not_defined();
  }
}

function foo() {
  try {
    function const_42() {
      return 42;
    }
    %PrepareFunctionForOptimization(const_42);
    const_42();
    bar(true);
  } catch {
  }
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
foo();

%OptimizeMaglevOnNextCall(foo);
foo();
