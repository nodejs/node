// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-wasm-lazy-compilation

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction(undefined, kSig_v_v)
  .addLocals(kWasmI32, 75)
  .addBody([
kExprTry, 0x40,  // try
  kExprLocalGet, 0x3d,  // local.get
  kExprI32Const, 0x2e,  // i32.const
  kExprI32GeS,  // i32.ge_s
  kExprIf, 0x40,  // if
    kExprCallFunction, 0x00,  // call function #0: v_v
    kExprUnreachable,  // unreachable
    kExprEnd,  // end
  kExprUnreachable,  // unreachable
  kExprEnd,  // end
kExprUnreachable,  // unreachable
]);
builder.toModule();
