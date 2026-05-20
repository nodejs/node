// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff --no-wasm-lazy-compilation

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const sig_index = builder.addType(makeSig([kWasmExternRef], []));
const table = builder.addTable(kWasmExternRef, 10);
builder.addFunction("main", makeSig([kWasmExternRef], []))
  .addBody([
      kExprI32Const, 0,
      kExprLocalGet, 0,
      kExprTableSet, table.index
  ])
  .exportFunc();
const buffer = builder.toBuffer();
const module_obj = new WebAssembly.Module(buffer);
const serialized = d8.wasm.serializeModule(module_obj);
const buffer_bytes = new Uint8Array(buffer);
for(let i=0; i<1000; i++) {
   if (buffer_bytes[i] == 0x60 && buffer_bytes[i+1] == 0x01 && buffer_bytes[i+2] == 0x6f && buffer_bytes[i+3] == 0x00) {
       type_offset = i + 2;
   }
}
const wire_bytes_sab = new SharedArrayBuffer(buffer.byteLength);
const wire_bytes_view = new Uint8Array(wire_bytes_sab);
wire_bytes_view.set(buffer_bytes);
const workerScript = `
  onmessage = function(e) {
    const msg = e.data;
    const sab = msg.sab;
    const view = new Int8Array(sab);
    const offset = msg.offset;
    const valid = 0x6f;
    const invalid = 0x7f;
    while(true) {
       Atomics.store(view, offset, valid);
       Atomics.store(view, offset, invalid);
    }
  };
`;
const worker = new Worker(workerScript, {type: 'string'});
worker.postMessage({sab: wire_bytes_sab, offset: type_offset});
const start = Date.now();
for (let i = 0; i < 10000 && Date.now() - start < 1000; i++) {
  try {
    const mod = d8.wasm.deserializeModule(serialized, wire_bytes_view);
    const instance = new WebAssembly.Instance(mod);
    instance.exports.main(0x41414141);
  } catch(e) {
  }
}
