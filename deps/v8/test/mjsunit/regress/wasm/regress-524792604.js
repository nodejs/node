// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --allow-natives-syntax --verify-heap
// Flags: --wasm-random-rescheduling --wasm-assert-types --no-liftoff

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.startRecGroup();
let $sig1 = builder.nextTypeIndex() + 1;
let $struct0 = builder.addStruct({fields: [makeField(wasmRefType($sig1), false), makeField(kWasmI64, true), makeField(kWasmI31Ref, true)]});
/* $sig1 */ builder.addType(makeSig([kWasmF64, kWasmI64, wasmRefType(kWasmExternRef), wasmRefType($struct0)], [wasmRefNullType($sig1)]));
builder.endRecGroup();
let $sig2 = builder.addType(kSig_v_v);
let $global1 = builder.addImportedGlobal('__fuzz_import', 'extern$_15', wasmRefType(kWasmExternRef));
let $func44 = builder.addFunction(undefined, $sig1);
let func_44_invoker = builder.addFunction(undefined, $sig2);
let $global31 = builder.addGlobal(kWasmI32, true, false, wasmI32Const(100));
let $segment1 = builder.addDeclarativeElementSegment([$func44.index]);

$func44.addLocals(wasmRefType($sig2), 1)  // $var4
  .addBody([
    kExprUnreachable,
  ]);

// func $func_44_invoker: [] -> []
func_44_invoker.addLocals(wasmRefType($sig1), 1)  // $var0
  .addLocals(kWasmI31Ref, 1)  // $var1
  .addLocals(kWasmI64, 1)  // $var2
  .addLocals(kWasmI32, 1)  // $var3
  .addBody([
    kExprGlobalGet, $global31.index,
    kExprI32Eqz,
    kExprIf, kWasmVoid,
      ...wasmI32Const(100),
      kExprGlobalSet, $global31.index,
      kExprUnreachable,
    kExprEnd,
    kExprGlobalGet, $global31.index,
    kExprI32Const, 1,
    kExprI32Sub,
    kExprGlobalSet, $global31.index,
    ...wasmF64Const(-4096.451),
    kExprI64Const, 58,
    kExprGlobalGet, $global1,
    kExprRefFunc, $func44.index,
    kExprLocalTee, 0,  // $var0
    ...wasmF32Const(-24),
    kExprI64UConvertF32,
    kExprLocalTee, 2,  // $var2
    kExprRefNull, kNullRefCode,
    kGCPrefix, kExprStructNew, $struct0,
    kExprCallFunction, $func44.index,
    kExprDrop,
  ]);

builder.addExport('func_44_invoker', func_44_invoker.index);

const imports = {'__fuzz_import': {}};

const instance = builder.instantiate(imports);
assertTraps(kTrapFloatUnrepresentable, instance.exports.func_44_invoker);
gc();
