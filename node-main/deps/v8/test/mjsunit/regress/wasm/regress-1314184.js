// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(16, 32);
builder.addFunction('main', kSig_i_iii)
    .addBodyWithEnd([
      kExprI32Const, 0x0,  // i32.const
      kExprMemorySize, 0,  // memory.size
      kExprI32Popcnt,      // i32.popcnt
      kExprI32Const, 0,    // i32.const
      kExprMemoryGrow, 0,  // memory.grow
      kExprI32GeU,         // i32.ge_u
      kExprBrIf, 0,        // br_if depth=0
      kExprMemorySize, 0,  // memory.size
      kExprUnreachable,    // unreachable
      kExprEnd,            // end
    ])
    .exportFunc();
const instance = builder.instantiate();
assertTraps(kExprUnreachable, () => instance.exports.main(1, 2, 3));
