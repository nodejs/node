// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --experimental-wasm-imported-strings-utf8
// Flags: --wasm-random-rescheduling

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let $array0 = builder.addArray(kWasmI8, {final: true});
builder.startRecGroup();
let $sig6 = builder.addType(kSig_i_iii);
builder.endRecGroup();
let $sig24 = builder.addType(makeSig(
  [wasmRefNullType($array0), kWasmI32, kWasmI32],
  [wasmRefType(kWasmExternRef)]
));
let decodeStringFromUTF8Array =
  builder.addImport('wasm:text-decoder', 'decodeStringFromUTF8Array', $sig24);
let main = builder.addFunction(undefined, $sig6).exportAs('main');
let $data0 = builder.addPassiveDataSegment([
  79, 182, 237, 203, 23, 4, 26, 73, 219, 119, 96, 60, 82, 25, 18, 94, 129, 132,
  116, 29, 75, 223, 87, 228, 99, 8, 249, 229, 140, 48, 230, 177
]);

// func $main: [kWasmI32, kWasmI32, kWasmI32] -> [kWasmI32]
main.addLocals(kWasmI32, 9)  // $var3
  .addLocals(wasmRefType(kWasmExternRef), 1)  // $var12
  .addLocals(kWasmI64, 1)  // $var13
  .addLocals(wasmRefType(kWasmFuncRef), 1)  // $var14
  .addBody([
    kExprI32Const, 32,
    kExprI32Const, 0,
    kGCPrefix, kExprArrayNewData, $array0, $data0,
    ...wasmI32Const(2145),
    kExprI32Const, 1,
    kExprCallFunction, decodeStringFromUTF8Array,
    kExprLocalSet, 12,  // $var12
    kExprRefNull, kFuncRefCode,
    kGCPrefix, kExprRefCast, kFuncRefCode,
    kExprLocalSet, 14,  // $var14
    ...wasmI32Const(7746425),
  ]);

let kBuiltins = { builtins: ['text-decoder'] };
const instance = builder.instantiate({}, kBuiltins);
const wasm = instance.exports;
assertTraps(kTrapArrayOutOfBounds, wasm.main);
%WasmTierUpFunction(wasm.main);
assertTraps(kTrapArrayOutOfBounds, wasm.main);
