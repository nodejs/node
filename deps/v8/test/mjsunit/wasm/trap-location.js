// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

// Collect the Callsite objects instead of just a string:
Error.prepareStackTrace = function(error, frames) {
  return frames;
};

var builder = new WasmModuleBuilder();

var sig_index = builder.addType(kSig_i_v)

// Build a function to resemble this code:
//   if (idx < 2) {
//     return load(-2 / idx);
//   } else if (idx == 2) {
//     unreachable;
//   } else {
//     return call_indirect(idx);
//   }
// There are four different traps which are triggered by different input values:
// (0) division by zero; (1) mem oob; (2) unreachable; (3) invalid call target
// Each of them also has a different location where it traps.
builder.addFunction("main", kSig_i_i)
  .addBody([
      // offset 1
      kExprBlock,
            kExprGetLocal, 0,
            kExprI32Const, 2,
          kExprI32LtU,
        kExprIf,
        // offset 8
              kExprI32Const, 0x7e /* -2 */,
              kExprGetLocal, 0,
            kExprI32DivU,
          // offset 13
          kExprI32LoadMem, 0, 0,
          kExprBr, 1, 1,
        kExprEnd,
        // offset 20
            kExprGetLocal, 0,
            kExprI32Const, 2,
          kExprI32Eq,
        kExprIf,
          kExprUnreachable,
        kExprEnd,
        // offset 28
          kExprGetLocal, 0,
        kExprCallIndirect, kArity0, sig_index,
      kExprEnd,
  ])
  .exportAs("main");

var module = builder.instantiate();

function testWasmTrap(value, reason, position) {
  try {
    module.exports.main(value);
    fail("expected wasm exception");
  } catch (e) {
    assertEquals(kTrapMsgs[reason], e.message, "trap reason");
    assertEquals(3, e.stack.length, "number of frames");
    assertEquals(0, e.stack[0].getLineNumber(), "wasmFunctionIndex");
    assertEquals(position, e.stack[0].getPosition(), "position");
  }
}

// The actual tests:
testWasmTrap(0, kTrapDivByZero,      12);
testWasmTrap(1, kTrapMemOutOfBounds, 13);
testWasmTrap(2, kTrapUnreachable,    26);
testWasmTrap(3, kTrapFuncInvalid,    30);
