// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-shared --no-wasm-lazy-compilation

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let $array0 = builder.addArray(kWasmI8);
let $struct = builder.addStruct(
    {fields: [makeField(wasmRefNullType($array0), true)]});
let main = builder.addFunction(undefined, kSig_v_v).exportAs('main');
main.addLocals(wasmRefNullType($struct), 1)
  .addBody([
    kExprRefNull, ...wasmSignedLeb($struct),
    kExprRefNull, ...wasmSignedLeb($array0),
    kAtomicPrefix, kExprStructAtomicExchange, kAtomicAcqRel, $struct, 0,
    kGCPrefix, kExprRefCastNull, $struct,
    kExprLocalSet, 0,
  ]);
builder.instantiate({});
