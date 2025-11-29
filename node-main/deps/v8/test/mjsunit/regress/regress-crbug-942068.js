// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(index, array) {
  return index in array;
};
%PrepareFunctionForOptimization(foo);
let arr = [];
arr.__proto__ = [0];
assertFalse(foo(0, {}));
assertTrue(foo(0, arr));
assertFalse(foo(0, {}));
%OptimizeFunctionOnNextCall(foo);
assertTrue(foo(0, arr));
