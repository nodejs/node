// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let $sig0 = builder.addType(kSig_v_v);
let $sig1 = builder.addType(makeSig([], [kWasmExnRef]));
let $sig2 = builder.addType(makeSig([kWasmF32, kWasmI32, kWasmI32], [kWasmFuncRef, kWasmStructRef, kWasmNullExnRef, kWasmF32]));
let w0 = builder.addFunction(undefined, kSig_v_v).exportAs('w0');
builder.addStart(w0.index);

w0.addLocals(kWasmNullRef, 1)  // $var1
  .addLocals(kWasmF32, 1)  // $var3
  .addLocals(kWasmNullExnRef, 4)  // $var4
  .addBody([
    kExprRefNull, kNullRefCode,
    kExprLocalSet, 0,  // $var1
    kExprI32Const, 0,
    kExprF32ReinterpretI32,
    kExprLocalSet, 1,
    kExprBlock, $sig1,
      kExprLocalGet, 1,
      ...wasmI32Const(-2075409857),
      kExprRefNull, kNullRefCode,
      kGCPrefix, kExprRefTest, kArrayRefCode,
      kExprTryTable, $sig2, 1,
            kCatchAllRef, 0,
        kExprDrop,
        kExprDrop,
        kExprDrop,
        kExprRefNull, kExnRefCode,
        kExprThrowRef,
        kExprRefNull, kFuncRefCode,
        kExprRefNull, kNullRefCode,
        kExprRefNull, kNullExnRefCode,
        kExprLocalGet, 1,
      kExprEnd,
      kExprDrop,
      kExprLocalSet, 2,
      kExprDrop,
      kExprDrop,
      kExprLocalGet, 2,
    kExprEnd,
    kGCPrefix, kExprRefCastNull, kNullExnRefCode,
    kExprDrop,
  ]);

assertThrows(() => builder.instantiate(), WebAssembly.RuntimeError);
