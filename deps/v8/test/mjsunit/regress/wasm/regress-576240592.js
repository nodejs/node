// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --wasm-test-streaming

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const worker = new Worker(function() {
  postMessage(0);
  while (true) {
    %EstimateCurrentMemoryConsumption();
  }
}, {type: 'function'});
worker.getMessage();

const promises = [];
for (let i = 0; i < 2; i++) {
  const builder = new WasmModuleBuilder();
  builder.addGlobal(kWasmI32, false, false, wasmI32Const(i));
  builder.addFunction('f', kSig_v_v).addBody([]);
  for (let j = 0; j < 100; j++) {
    builder.addPassiveDataSegment([1, 2, 3, 4, 5, 6, 7, 8]);
  }
  promises.push(WebAssembly.compile(builder.toBuffer()));
}
assertPromiseResult(Promise.all(promises), () => worker.terminate());
