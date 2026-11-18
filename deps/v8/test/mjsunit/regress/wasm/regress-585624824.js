// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let array;
let truncate = false;
function sideEffect() {
  if (truncate) array.length = 0;
}

const builder = new WasmModuleBuilder();
const side_effect = builder.addImport('m', 'sideEffect', kSig_v_v);
builder.addFunction('compare', kSig_i_ii)
    .addBody([
      kExprCallFunction, side_effect,
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprI32Sub,
    ])
    .exportFunc();
const compare = builder.instantiate({m: {sideEffect}}).exports.compare;

// Returns a PACKED_ELEMENTS array.
function makeArray(last) {
  const a = [3, 2, {}];
  a[2] = last;
  return a;
}

function sort(a) {
  return a.sort(compare);
}

%PrepareFunctionForOptimization(sort);
sort(makeArray(1));
sort(makeArray(1));
%OptimizeFunctionOnNextCall(sort);
sort(makeArray(1));

// The first comparison truncates the receiver.  The second one deopts while
// converting "1" to i32, before compare is entered.  The sort must continue
// from its snapshot of the elements.
truncate = true;
array = makeArray('1');
assertEquals(['1', 2, 3], sort(array));
