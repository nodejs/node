// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --dump-counters

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function testWasm() {
  // Regression test for https://crbug.com/v8/12453.
  if (typeof WebAssembly === 'undefined') return;  // Skip on jitless.
  const builder = new WasmModuleBuilder();
  builder.addFunction('f', kSig_v_v).addBody([]);
  builder.instantiate();
})();
