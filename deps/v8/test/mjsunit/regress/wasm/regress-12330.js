// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbo-force-mid-tier-regalloc

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addType(makeSig([], [kWasmF32]));
builder.addFunction(undefined, 0 /* sig */)
  .addLocals(kWasmI32, 1)
  .addBodyWithEnd([
// signature: f_v
// body:
kExprLoop, 0x7d,  // loop @3 f32
  kExprI32Const, 0x9a, 0x7f,  // i32.const
  kExprI32Const, 0x01,  // i32.const
  kExprLocalGet, 0x00,  // local.get
  kExprSelect,  // select
  kExprLocalTee, 0x00,  // local.tee
  kExprBrIf, 0x00,  // br_if depth=0
  kExprF32Const, 0x4e, 0x03, 0xf1, 0xff,  // f32.const
  kExprEnd,  // end @22
kExprEnd,  // end @23
]);
builder.toModule();
