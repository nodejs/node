// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --jitless --wasm-jitless

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction('test', kSig_v_v)
    .addBody([
      kExprTryTable, kWasmVoid, 0,
      kExprEnd,
    ])
    .exportFunc();

const wire_bytes = builder.toBuffer();
assertFalse(WebAssembly.validate(wire_bytes));
assertThrows(
    () => new WebAssembly.Module(wire_bytes), WebAssembly.CompileError,
    /exnref not supported by the Wasm interpreter/);
