// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-staging

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(28, 32, false);
builder.addFunction(undefined, kSig_i_v)
  .addLocals(kWasmI32, 61)
  .addBody([
kExprI64Const, 0x0,  // i64.const
kExprI32Const, 0x0,  // i32.const
kExprIf, kWasmStmt,  // if
  kExprI32Const, 0x0,  // i32.const
  kExprI32LoadMem, 0x01, 0x23,  // i32.load
  kExprBrTable, 0x01, 0x00, 0x00, // br_table
  kExprEnd,  // end
kExprI64SExtendI16,  // i64.extend16_s
kExprI32Const, 0x00,  // i32.const
kExprLocalGet, 0x00,  // local.get
kExprI32StoreMem16, 0x00, 0x10,  // i32.store16
kExprUnreachable,  // unreachable
]).exportAs('main');
const instance = builder.instantiate();
assertThrows(instance.exports.main, WebAssembly.RuntimeError, 'unreachable');
