// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function foo(index) {
  const array = new Uint8Array(8);
  array[index] = 42;
  return array;
}

%PrepareFunctionForOptimization(foo);

foo(0);
foo(0);

%OptimizeMaglevOnNextCall(foo);

foo(0);

// This call is one element past the end and should deoptimize.
foo(8);
