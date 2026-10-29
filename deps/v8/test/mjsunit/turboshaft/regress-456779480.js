// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

let o = { x : "abc" };

function foo() {
  o.x = arguments[0];
}

%PrepareFunctionForOptimization(foo);
foo(42);
foo(42);

%OptimizeFunctionOnNextCall(foo);
foo(42);
