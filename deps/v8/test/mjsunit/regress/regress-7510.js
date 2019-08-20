// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

function foo(a) {
  for (const x of a) {
    a[100] = 1;
  }
}

%PrepareFunctionForOptimization(foo);
foo([1]);
foo([1]);
%OptimizeFunctionOnNextCall(foo);
foo([1]);
%PrepareFunctionForOptimization(foo);
%OptimizeFunctionOnNextCall(foo);
foo([1]);
assertOptimized(foo);
