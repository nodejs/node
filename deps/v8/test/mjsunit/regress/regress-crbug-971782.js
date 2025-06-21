// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(dv) {
  for (let i = -1; i < 1; ++i) {
    dv.setUint16(i % 1);
  }
}

const dv = new DataView(new ArrayBuffer(2));
%PrepareFunctionForOptimization(foo);
foo(dv);
foo(dv);
%OptimizeFunctionOnNextCall(foo);
foo(dv);
