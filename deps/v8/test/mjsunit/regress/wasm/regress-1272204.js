// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff --turbo-force-mid-tier-regalloc

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(16, 32, false);
builder.addFunction(undefined, kSig_i_iii).addBody([
  kExprI64Const, 0x7a,  // i64.const
  kExprI64Const, 0x7f,  // i64.const
  kExprI64Const, 0x7e,  // i64.const
  kExprI64Add,          // i64.add
  kExprI64DivS,         // i64.div_s
  kExprUnreachable,     // unreachable
]);
builder.toModule();
