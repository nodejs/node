// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-eh

load("test/mjsunit/wasm/wasm-module-builder.js");

(function TestTruncatedBrOnExnInLoop() {
  let builder = new WasmModuleBuilder();
  let fun = builder.addFunction(undefined, kSig_v_v)
      .addLocals({except_count: 1})
      .addBody([
        kExprLoop, kWasmStmt,
          kExprLocalGet, 0,
          kExprBrOnExn  // Bytecode truncated here.
      ]).exportFunc();
  fun.body.pop();  // Pop implicitly added kExprEnd from body.
  assertThrows(() => builder.instantiate(), WebAssembly.CompileError);
})();
