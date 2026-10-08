// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-assert-types --wasm-random-rescheduling

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let $sig3 = builder.addType(kSig_v_v);
let $sig4 =
  builder.addType(makeSig([wasmRefType(kWasmAnyRef)], [kWasmFuncRef]));

let $array18 = builder.addArray(kWasmI8, {mutable: false, final: true});
let $sig26 = builder.addType(makeSig([kWasmExternRef], [kWasmFuncRef]));
let $sig37 = builder.addType(makeSig(
  [kWasmFuncRef],
  [wasmRefNullType($sig4), kWasmI32, kWasmS128, kWasmS128, kWasmF64]));
let $func8 = builder.addFunction(undefined, $sig4);
let $func14 = builder.addFunction(undefined, $sig4);
let func_22 = builder.addFunction(undefined, $sig37);
let func_41_invoker = builder.addFunction(undefined, $sig3);
let $func40 = builder.addFunction(undefined, $sig26);
let $mem0 = builder.addMemory(16, 17, true);
let $global4 = builder.addGlobal(kWasmI32, true, false, wasmI32Const(100));
let $segment1 = builder.addDeclarativeElementSegment([$func14.index]);

// func $func8: [wasmRefType(kWasmAnyRef)] -> [kWasmFuncRef]
$func8.addBody([
    kExprGlobalGet, $global4.index,
    kExprI32Eqz,
    kExprIf, kWasmVoid,
      ...wasmI32Const(100),
      kExprGlobalSet, $global4.index,
      kExprUnreachable,
    kExprEnd,
    kExprGlobalGet, $global4.index,
    kExprI32Const, 1,
    kExprI32Sub,
    kExprGlobalSet, $global4.index,
    kExprCallFunction, func_41_invoker.index,
    kExprLocalGet, 0,  // $var0
    kExprRefFunc, $func14.index,
    kExprCallRef, $sig4,
  ]);

$func14
  .addBody([
    kExprRefFunc, $func14.index,
  ]);

func_22.addBody([
    kExprUnreachable,
  ]);

// func $func_41_invoker: [] -> []
func_41_invoker.addBody([
    kExprGlobalGet, $global4.index,
    kExprI32Eqz,
    kExprIf, kWasmVoid,
      ...wasmI32Const(100),
      kExprGlobalSet, $global4.index,
      kExprUnreachable,
    kExprEnd,
    kExprGlobalGet, $global4.index,
    kExprI32Const, 1,
    kExprI32Sub,
    kExprGlobalSet, $global4.index,
    kGCPrefix, kExprArrayNewFixed, $array18, 0,
    kExprDrop,
  ]);

// func $test-A: [kWasmExternRef] -> [kWasmFuncRef]
$func40.addBody([
    kExprLocalGet, 0,  // $var0
    kGCPrefix, kExprAnyConvertExtern,
    kGCPrefix, kExprRefCast, kAnyRefCode,
    kExprCallFunction, $func8.index,
  ]);

builder.addExport('test-A', $func40.index);
builder.addExport('func_22', func_22.index);

const instance = builder.instantiate({});

assertTraps(kTrapIllegalCast, () => instance.exports['test-A'](null));
