// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-staging

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(1, 1, false, true);
builder.addGlobal(kWasmI32, 1);
const sig = builder.addType(makeSig([kWasmI32, kWasmI64, kWasmI64, kWasmI64], [kWasmF32]));
// Generate function 1 (out of 3).
builder.addFunction(undefined, sig)
  .addLocals({i32_count: 57}).addLocals({i64_count: 11})
  .addBodyWithEnd([
// signature: f_illl
// body:
kExprLocalGet, 0x1b,  // local.get
kExprLocalSet, 0x1c,  // local.set
kExprI32Const, 0x00,  // i32.const
kExprIf, kWasmStmt,  // if @11
  kExprGlobalGet, 0x00,  // global.get
  kExprLocalSet, 0x1e,  // local.set
  kExprBlock, kWasmStmt,  // block @19
    kExprGlobalGet, 0x00,  // global.get
    kExprLocalSet, 0x21,  // local.set
    kExprBlock, kWasmStmt,  // block @25
      kExprBlock, kWasmStmt,  // block @27
        kExprBlock, kWasmStmt,  // block @29
          kExprGlobalGet, 0x00,  // global.get
          kExprLocalSet, 0x0a,  // local.set
          kExprI32Const, 0x00,  // i32.const
          kExprLocalSet, 0x28,  // local.set
          kExprLocalGet, 0x00,  // local.get
          kExprLocalSet, 0x0b,  // local.set
          kExprI32Const, 0x00,  // i32.const
          kExprBrIf, 0x01,  // br_if depth=1
          kExprEnd,  // end @47
        kExprUnreachable,  // unreachable
        kExprEnd,  // end @49
      kExprI32Const, 0x01,  // i32.const
      kExprLocalSet, 0x36,  // local.set
      kExprI32Const, 0x00,  // i32.const
      kExprIf, kWasmStmt,  // if @56
        kExprEnd,  // end @59
      kExprLocalGet, 0x00,  // local.get
      kExprLocalSet, 0x10,  // local.set
      kExprI32Const, 0x00,  // i32.const
      kExprI32Eqz,  // i32.eqz
      kExprLocalSet, 0x38,  // local.set
      kExprBlock, kWasmStmt,  // block @69
        kExprI32Const, 0x7f,  // i32.const
        kExprI32Eqz,  // i32.eqz
        kExprLocalSet, 0x39,  // local.set
        kExprI32Const, 0x01,  // i32.const
        kExprIf, kWasmStmt,  // if @78
          kExprGlobalGet, 0x00,  // global.get
          kExprLocalSet, 0x11,  // local.set
          kExprI32Const, 0x00,  // i32.const
          kExprI32Eqz,  // i32.eqz
          kExprLocalSet, 0x12,  // local.set
          kExprGlobalGet, 0x00,  // global.get
          kExprLocalSet, 0x13,  // local.set
          kExprI32Const, 0x00,  // i32.const
          kExprI32Const, 0x01,  // i32.const
          kExprI32Sub,  // i32.sub
          kExprLocalSet, 0x3a,  // local.set
          kExprI32Const, 0x00,  // i32.const
          kAtomicPrefix, kExprI64AtomicLoad16U, 0x01, 0x04,  // i64.atomic.load16_u
          kExprDrop,  // drop
          kExprI64Const, 0x01,  // i64.const
          kExprLocalSet, 0x44,  // local.set
          kExprI64Const, 0x01,  // i64.const
          kExprLocalSet, 0x3e,  // local.set
        kExprElse,  // else @115
          kExprNop,  // nop
          kExprEnd,  // end @117
        kExprLocalGet, 0x40,  // local.get
        kExprLocalSet, 0x41,  // local.set
        kExprLocalGet, 0x41,  // local.get
        kExprI64Const, 0x4b,  // i64.const
        kExprI64Add,  // i64.add
        kExprDrop,  // drop
        kExprEnd,  // end @128
      kExprEnd,  // end @129
    kExprUnreachable,  // unreachable
    kExprEnd,  // end @132
  kExprUnreachable,  // unreachable
  kExprEnd,  // end @134
kExprF32Const, 0x00, 0x00, 0x84, 0x42,  // f32.const
kExprEnd,  // end @140
]);
const instance = builder.instantiate();
