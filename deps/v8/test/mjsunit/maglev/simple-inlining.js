// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --maglev-inlining

function global_func(x) {
  return x;
}

function foo(x) {
  return global_func(x);
}

%PrepareFunctionForOptimization(foo);
%PrepareFunctionForOptimization(global_func);
print(foo(1));
print(foo(1));
%OptimizeMaglevOnNextCall(foo);
print(foo(1));
