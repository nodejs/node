// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --verify-heap --allow-natives-syntax

// This checks that TransitionAndStoreNumberElement silences NaNs.
function foo() {
  return [undefined].map(Math.asin);
};
%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();

// This checks that TransitionAndStoreElement silences NaNs.
function bar(b) {
  return [undefined].map(x => b ? Math.asin(x) : "string");
};
%PrepareFunctionForOptimization(bar);
bar(true);
bar(false);
bar(true);
bar(false);
%OptimizeFunctionOnNextCall(bar);
bar(true);
