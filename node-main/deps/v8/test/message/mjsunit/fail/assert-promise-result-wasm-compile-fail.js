// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-stack-trace-frames

// Test wasm compilation explicitly, since this creates a promise which is only
// resolved later, i.e. the message queue gets empty in-between.
// The important part here is that d8 exits with a non-zero exit code.

d8.file.execute('test/mjsunit/mjsunit.js');
d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

assertPromiseResult((async function test() {
  let ok_buffer = (() => {
    let builder = new WasmModuleBuilder();
    builder.addFunction(undefined, kSig_i_v).addBody([kExprI32Const, 42]);
    return builder.toBuffer();
  })();
  let bad_buffer = new ArrayBuffer(0);
  let kNumCompiles = 3;

  // Three compilations of the OK module should succeed.
  for (var i = 0; i < kNumCompiles; ++i) {
    await WebAssembly.compile(ok_buffer);
  }

  // Three compilations of the bad module should fail.
  for (var i = 0; i < kNumCompiles; ++i) {
    await WebAssembly.compile(bad_buffer);
  }
})());
