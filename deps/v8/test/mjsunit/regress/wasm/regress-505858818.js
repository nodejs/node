// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Combining --liftoff-only with --enable-testing-opcode-in-wasm should not
// cause crashes when fuzzing.
// Flags: --fuzzing --liftoff-only --enable-testing-opcode-in-wasm

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
builder.addFunction("test", kSig_v_v)
  .addBody([kExprNopForTestingUnsupportedInLiftoff])
  .exportFunc();
let errorMsg = new RegExp(
  `Invalid opcode 0x${kExprNopForTestingUnsupportedInLiftoff.toString(16)}`);
assertThrows(() => builder.instantiate().exports.test(),
  WebAssembly.CompileError, errorMsg);
