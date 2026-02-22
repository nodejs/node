// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

let count = 0;

const view = new DataView(new ArrayBuffer(16));
function foo(arg) {
  for (var i = 0; i < arg.byteLength; i++) {
    arg.setInt8(i);
    count++;
  }
}
%PrepareFunctionForOptimization(foo);
foo(view);
%OptimizeMaglevOnNextCall(foo);
foo(view);

assertEquals(32, count);
