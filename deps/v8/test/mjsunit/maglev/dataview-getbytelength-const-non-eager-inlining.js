// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev
// Flags: --max-maglev-inlined-bytecode-size-small=0
// Flags: --maglev-non-eager-inlining

let ab = new ArrayBuffer(100);
let dv = new DataView(ab);

// --max-maglev-inlined-bytecode-size-small=0 prevents this from getting eagerly inlined.
function getDataView() {
  return dv;
}
%PrepareFunctionForOptimization(getDataView);

var g1 = 80;
var g2 = 70;
// Make these not const.
g1 = 81;
g2 = 71;

function foo() {
  g1 = getDataView().byteLength;
  // Reading the byteLength the second time should be optimized out.
  g2 = getDataView().byteLength;
}
%PrepareFunctionForOptimization(foo);

foo();

%OptimizeMaglevOnNextCall(foo);
foo();

assertEquals(100, g1);
assertEquals(100, g2);
