// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --liftoff --no-wasm-tier-up --no-future --wasm-tier-mask-for-testing=2

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(1, 1);
// First function is Liftoff. The first parameter is used as memory offset.
builder.addFunction(undefined, kSig_v_i).addBody([
  kExprGetLocal, 0,        // get_local 0
  kExprI32Const, 0,        // i32.const 0
  kExprI32StoreMem, 0, 0,  // i32.store offset=0
]);
// Second function is Turbofan. It loads the sixth parameter from the stack
// into a register for the first argument. Even though it's a 32-bit value, it
// is loaded as 64-bit value on x64.
builder.addFunction(undefined, makeSig(new Array(6).fill(kWasmI32), []))
    .addBody([
      kExprGetLocal, 5,     // get_local 5
      kExprCallFunction, 0  // call 0
    ]);
// The third function is Liftoff again. A value is spilled on the stack as i32,
// then used as a call argument, passed via the stack. The full 64-bit are
// copied on the stack, even though just 32-bit were written before. Hence, the
// stack slot is not zero-extended.
const gen_i32_code = [
  kExprTeeLocal, 0,  // tee_local 0
  kExprGetLocal, 0,  // get_local 0
  kExprI32Const, 1,  // i32.const 1
  kExprI32Add        // i32.add     --> 2nd param
];
builder.addFunction(undefined, kSig_v_v).addLocals({i32_count: 1}).addBody([
  // Generate six values on the stack, then six more to force the other six on
  // the stack.
  ...wasmI32Const(0),    // i32.const 0
  ...wasmI32Const(1),    // i32.const 1
  kExprI32Add,           // i32.add --> 1st param
  ...gen_i32_code,       // --> 2nd param
  ...gen_i32_code,       // --> 3rd param
  ...gen_i32_code,       // --> 4th param
  ...gen_i32_code,       // --> 5th param
  ...gen_i32_code,       // --> 6th param
  ...gen_i32_code,       // --> garbage
  ...gen_i32_code,       // --> garbage
  ...gen_i32_code,       // --> garbage
  ...gen_i32_code,       // --> garbage
  ...gen_i32_code,       // --> garbage
  ...gen_i32_code,       // --> garbage
  kExprDrop,             // drop garbage
  kExprDrop,             // drop garbage
  kExprDrop,             // drop garbage
  kExprDrop,             // drop garbage
  kExprDrop,             // drop garbage
  kExprDrop,             // drop garbage
  kExprCallFunction,  1  // call 1
]).exportAs('three');
const instance = builder.instantiate();
instance.exports.three();
