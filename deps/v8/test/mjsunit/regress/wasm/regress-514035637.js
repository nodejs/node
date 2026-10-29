// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-assert-types

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.startRecGroup();
let $array0 = builder.addArray(wasmRefType(kWasmNullRef), {final: true});
let $array1 = builder.addArray(kWasmI16, {mutable: false, final: true});
builder.endRecGroup();
builder.startRecGroup();
let $sig2 = builder.addType(kSig_v_v);
builder.endRecGroup();
let w0 = builder.addFunction(undefined, $sig2).exportAs('w0');

w0.addBody([
    kGCPrefix, kExprArrayNewFixed, $array0, 0,
    kExprI32Const, 0,
    kGCPrefix, kExprArrayGet, $array0,
    kGCPrefix, kExprRefCast, $array1,
    kExprI32Const, 0,
    kGCPrefix, kExprArrayGetS, $array1,
    kExprDrop,
  ]);

let kBuiltins = { builtins: ['js-string', 'text-decoder', 'text-encoder'] };
const instance = builder.instantiate({}, kBuiltins);
assertTraps(kTrapArrayOutOfBounds, () => instance.exports.w0());
