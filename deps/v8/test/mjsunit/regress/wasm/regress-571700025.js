// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff --no-wasm-inlining --expose-gc

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const struct_type = builder.addStruct([makeField(kWasmI32, true)]);
const ref_type = wasmRefType(struct_type);
const sig = makeSig([kWasmI32], [kWasmI32, kWasmI32, ref_type]);
const gc_idx = builder.addImport('m', 'gc', kSig_v_v);

const callee = builder.addFunction('callee', sig).addBody([
  kExprLocalGet, 0,
  kExprLocalGet, 0,
  kExprLocalGet, 0,
  kGCPrefix, kExprStructNew, struct_type,
]);

builder.addFunction('caller', kSig_i_i).addBody([
  kExprLocalGet, 0,
  kExprCallFunction, callee.index,
  kExprCallFunction, gc_idx,
  kGCPrefix, kExprStructGet, struct_type, 0,
  kExprI32Add,
  kExprI32Add,
]).exportFunc();

const instance = builder.instantiate({
  m: {
    gc: () => {
      for (let i = 0; i < 10000; i++) new Array(100);
      gc();
    }
  }
});

assertEquals(126, instance.exports.caller(42));
