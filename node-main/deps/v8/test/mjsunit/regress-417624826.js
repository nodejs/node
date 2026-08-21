// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(x) {
  x + x * 2 ** 52;
}

%PrepareFunctionForOptimization(foo);
foo(0); // Since Ignition does not canonicalize smis
        // feedback end it up being additive safe int.

%OptimizeFunctionOnNextCall(foo);
function bar() {
  foo(1);
}

%PrepareFunctionForOptimization(bar);
bar(); // Since foo is optimized we don't update feedback.
%OptimizeFunctionOnNextCall(bar);
bar(); // Optimize with input to add op equal to 2**52 and
       // feedback is additive safe int.
