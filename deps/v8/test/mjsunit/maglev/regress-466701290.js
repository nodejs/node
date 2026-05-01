// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

let ab = new ArrayBuffer(8);
let view = new DataView(ab);

function foo(dv) {
  let len = dv.byteLength;
  dv.getFloat64(0);
  return len;
}

%PrepareFunctionForOptimization(foo);
assertEquals(8, foo(view));
%OptimizeMaglevOnNextCall(foo);
assertEquals(8, foo(view));
