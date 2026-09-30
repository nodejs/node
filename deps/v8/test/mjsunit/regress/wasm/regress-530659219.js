// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --stress-compaction --verify-heap --expose-gc

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const N = 12;

const builder = new WasmModuleBuilder();
const $I = builder.addStruct([makeField(kWasmI32, true)]);
const $A = builder.addArray(wasmRefType($I), {mutable: true});

const init = [];
for (let i = 0; i < N; i++) {
  init.push(kGCPrefix, kExprStructNewDefault, $I);
}
init.push(kGCPrefix, kExprArrayNewFixed, $A, N);

const g = builder.addGlobal(wasmRefType($A), false, false, init);

builder.addFunction("get", makeSig([kWasmI32], [kWasmI32]))
  .addBody([
    kExprGlobalGet, g.index,
    kExprLocalGet, 0,
    kGCPrefix, kExprArrayGet, $A,
    kGCPrefix, kExprStructGet, $I, 0,
  ])
  .exportFunc();

builder.addFunction("elem", makeSig([kWasmI32], [kWasmExternRef]))
  .addBody([
    kExprGlobalGet, g.index,
    kExprLocalGet, 0,
    kGCPrefix, kExprArrayGet, $A,
    kGCPrefix, kExprExternConvertAny,
  ])
  .exportFunc();

const inst = builder.instantiate();

gc();

for (let i = 0; i < N; i++) {
  let e = inst.exports.elem(i);
  %HeapObjectVerify(e);
  let v = inst.exports.get(i);
  assertEquals(0, v);
}
