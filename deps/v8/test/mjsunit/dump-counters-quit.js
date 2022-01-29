// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --dump-counters

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Test that if two isolates are running (with the --isolates flag on the test
// runner) and one of them calls `quit`, the other one can still write to
// counters concurrently.

if (typeof WebAssembly !== 'undefined') {  // Skip on jitless.
  const builder = new WasmModuleBuilder();
  builder.addFunction('f', kSig_v_v).addBody([]);
  builder.asyncInstantiate();
}
quit();
