// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --trace-wasm

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();

builder.addFunction("foo", kSig_v_r).addLocals(kWasmI32, 1).addBody([
    kExprLoop, kWasmVoid,
      kExprLocalGet, 1,
      kExprBrIf, 1,  // Conditional return.
    kExprEnd,
  ]).exportFunc();

let instance = builder.instantiate();
instance.exports.foo(null);
