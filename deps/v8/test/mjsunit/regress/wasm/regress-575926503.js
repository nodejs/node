// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --log-code --vtune-prof-annotate-wasm

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const worker = new Worker(function() {
  postMessage(0);
  while (true) {
    %EstimateCurrentMemoryConsumption();
  }
}, {type: 'function'});
worker.getMessage();

const builder = new WasmModuleBuilder();
builder.addFunction('f', kSig_v_v).addBody([]);
const url = 'test/mjsunit/regress/wasm/regress-575926503.js';
const url_bytes = Array.from(url, c => c.charCodeAt(0));
builder.addCustomSection('sourceMappingURL', [url_bytes.length, ...url_bytes]);
builder.addCustomSection('id', [0, 0]);
const wire_bytes = new Uint8Array(builder.toBuffer());

for (let i = 0; i < 10; i++) {
  wire_bytes[wire_bytes.length - 1] = i & 0xff;
  wire_bytes[wire_bytes.length - 2] = (i >> 8) & 0xff;
  new WebAssembly.Module(wire_bytes);
}
worker.terminate();
