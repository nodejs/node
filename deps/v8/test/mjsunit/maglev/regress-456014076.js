// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function bar(a = true) {
  try {} catch (e) {}
}

function foo() {
  try {
    const x = bar();
    "foo"[y];
    const y = z = () => x; 337459, {
      get: function () { y; },
    };
  } catch (e) {}
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
