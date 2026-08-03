// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();

builder.addMemory(16, 17);

builder.addTag(kSig_v_f);

builder.addFunction('func1', kSig_v_v)
  .addBody([
    kExprLoop, 0x40,
      kExprTry, 0x40,
        kExprF32Const, 0x81, 0xe2, 0xa0, 0x7a,
        kExprThrow, 0x00,
      kExprCatchAll,
      kExprEnd,
    kExprEnd,
    kExprI32Const, 0x3b,
    kExprI64Const, 0x3b,
    kExprI64StoreMem, 0x01, 0x80, 0xfe, 0xff, 0x6f]);

builder.instantiate();
