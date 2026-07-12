// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

try {
  let array = new ArrayBuffer();
  array.constructor = {
    get [Symbol.species]() {
      %ArrayBufferDetach(array);
    }
  };
  array.slice();
} catch (e) {}

var array = new Int8Array(100);
function foo() {
  for (var i = 0; i < 100; i += 4) {
    array[i] = i;
  }
}

%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
