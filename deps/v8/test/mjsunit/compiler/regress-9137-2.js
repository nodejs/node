// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt
// Flags: --no-flush-bytecode --no-stress-flush-bytecode

function changeMap(obj) {
  obj.blub = 42;
}

function reducer(acc, val, i, obj) {
  return changeMap(obj);
}

function foo(obj) {
  return obj.reduce(reducer);
}

%NeverOptimizeFunction(reducer);
%PrepareFunctionForOptimization(foo);
foo([0, 1, 2]);
foo([0, 1, 2]);
%OptimizeFunctionOnNextCall(foo);
foo([0, 1, 2]);
%OptimizeFunctionOnNextCall(foo);
foo([0, 1, 2]);
assertOptimized(foo);
