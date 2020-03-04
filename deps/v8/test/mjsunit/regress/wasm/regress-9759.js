// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-wasm-tier-up --no-liftoff --no-force-slow-path

load("test/mjsunit/wasm/wasm-module-builder.js");

// This constant was chosen as it is the smallest number of cases that still
// triggers the input count overflow. The new limit put into place is smaller.
const NUM_CASES = 0xfffd;

(function TestBrTableTooLarge() {
  let builder = new WasmModuleBuilder();
  let cases = new Array(NUM_CASES).fill(0);
  builder.addFunction('main', kSig_v_i)
      .addBody([].concat([
        kExprBlock, kWasmStmt,
          kExprLocalGet, 0,
          kExprBrTable], wasmSignedLeb(NUM_CASES),
          cases, [0,
        kExprEnd
      ])).exportFunc();
  assertThrows(() => new WebAssembly.Module(builder.toBuffer()),
    WebAssembly.CompileError, /invalid table count/);
})();
