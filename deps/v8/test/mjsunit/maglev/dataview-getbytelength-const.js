// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev
// Flags: --maglev-non-eager-inlining

let ab = new ArrayBuffer(100);
let dv = new DataView(ab);

var g1 = 80;
var g2 = 70;
// Make these not const.
g1 = 81;
g2 = 71;

function foo(dv) {
  g1 = dv.byteLength;
  // Reading the byteLength the second time should be optimized out.
  g2 = dv.byteLength;
}
%PrepareFunctionForOptimization(foo);

foo(dv);

%OptimizeMaglevOnNextCall(foo);
foo(dv);

assertEquals(100, g1);
assertEquals(100, g2);
