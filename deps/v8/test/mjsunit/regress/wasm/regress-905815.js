// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

(function() {
  const builder = new WasmModuleBuilder();
  builder.addType(makeSig([], []));
  builder.addType(makeSig([kWasmI32], [kWasmI32]));
  builder.addFunction(undefined, 0 /* sig */)
    .addBodyWithEnd([
        kExprEnd,   // @1
    ]);
  builder.addFunction(undefined, 1 /* sig */)
    .addLocals({i32_count: 65})
    .addBodyWithEnd([
        kExprLoop, kWasmStmt,   // @3
        kSimdPrefix,
        kExprF32x4Min,
        kExprI64UConvertI32,
        kExprI64RemS,
        kExprUnreachable,
        kExprLoop, 0x02,   // @10
    ]);
})
