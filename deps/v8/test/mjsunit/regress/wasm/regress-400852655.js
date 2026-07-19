// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff --no-wasm-lazy-compilation --no-wasm-opt

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let $struct0 = builder.addStruct([makeField(kWasmI32, false)], kNoSuperType, true);
let $sig1 = builder.addType(makeSig([kWasmI32], [wasmRefNullType($struct0)]));
let $func0 = builder.addFunction(undefined, $sig1);

// func $func0: [kWasmI32] -> [wasmRefNullType($struct0)]
$func0.addLocals(wasmRefNullType($struct0), 1)  // $var1
  .addLocals(kWasmI32, 2)  // $var2 - $var3
  .addBody([
    kExprLoop, kWasmVoid,
      kExprLocalGet, 0,  // $var0
      kExprI32Const, 0,
      kExprI32LtS,
      kExprBrIf, 0,
      kExprLocalGet, 0,  // $var0
      kGCPrefix, kExprStructNew, $struct0,
      kExprLocalSet, 1,  // $var1
      kExprI32Const, 0,
      kExprLocalSet, 3,  // $var3
      kExprLoop, kWasmVoid,
        kExprLocalGet, 3,  // $var3
        kExprI32Const, 1,
        kExprI32Add,
        kExprLocalSet, 3,  // $var3
        kExprLocalGet, 3,  // $var3
        kExprI32Const, 1,
        kExprI32LtS,
        kExprBrIf, 0,
      kExprEnd,
      kExprLocalGet, 2,  // $var2
      kExprI32Const, 1,
      kExprI32Add,
      kExprLocalSet, 2,  // $var2
      kExprLocalGet, 2,  // $var2
      kExprI32Const, 10,
      kExprI32LtS,
      kExprBrIf, 0,
    kExprEnd,
    kExprUnreachable,
  ]);

const instance = builder.instantiate({});
