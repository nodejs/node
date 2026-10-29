// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(arr) {
  var y = arr[59];
  var z = arr[59];
  return [undefined, y, z];
}

let arr = new Int32Array(64);

%PrepareFunctionForOptimization(foo);
assertEquals([undefined, 0, 0], foo(arr));
assertEquals([undefined, 0, 0], foo(arr));

%OptimizeMaglevOnNextCall(foo);
assertEquals([undefined, 0, 0], foo(arr));
