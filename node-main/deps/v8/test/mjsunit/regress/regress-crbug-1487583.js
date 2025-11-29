// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

// Triggers mutual inlining.
function bar(x) {
  foo([]);
}
function foo(arr) {
  arr.forEach(bar);
}

foo([]);
%PrepareFunctionForOptimization(foo);
%PrepareFunctionForOptimization(bar);
foo([]);
bar([]);
foo([]);
%OptimizeFunctionOnNextCall(foo);
foo([]);
