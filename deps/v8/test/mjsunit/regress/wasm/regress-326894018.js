// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-wasm-lazy-compilation

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let $sig_i_nn = builder.addType(makeSig([kWasmExternRef, kWasmExternRef], [kWasmI32]));
let $equals = builder.addImport('wasm:js-string', 'equals', $sig_i_nn);
builder.addMemory(16, 32);
builder.addTag(kSig_v_v);

builder.addFunction(undefined, kSig_i_iii)
  .addBodyWithEnd([
    kExprI32Const, 0xc7, 0x00,
    kExprTry, kWasmVoid,
      kExprLoop, kWasmVoid,
        kExprRefNull, kExternRefCode,
        kExprRefNull, kExternRefCode,
        kExprCallFunction, $equals,
        kExprIf, kWasmVoid,
          kExprBr, 1,
        kExprEnd,
      kExprEnd,  // end loop
      kExprCatch, 0,
      kExprEnd,  // end catch
      kSimdPrefix, kExprS128Load8x8U, 0x03, 0xc7, 0x8f, 0x03,
      kExprDrop,
      kExprI32Const, 0xd4, 0xde, 0x94, 0xff, 0x00,
    kExprEnd,
  ]);

let kBuiltins = { builtins: ['js-string'] };
const instance = builder.instantiate({}, kBuiltins);
