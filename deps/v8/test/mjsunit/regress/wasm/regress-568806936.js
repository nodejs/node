// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const kMaxPages = 500;
const kStateIndex = 0;
const kWorkerRunning = 0;
const kWorkerDone = 1;

const builder = new WasmModuleBuilder();
builder.addImportedMemory('mod', 'mem', 1, kMaxPages, 'shared');
const module = builder.toModule();

const mem =
    new WebAssembly.Memory({initial: 1, maximum: kMaxPages, shared: true});
const comm = new Int32Array(mem.toResizableBuffer());

function workerCode() {
  onmessage = function({data}) {
    postMessage('started');
    for (let i = 1; i < data.kMaxPages; i++) {
      data.mem.grow(1);
    }
    Atomics.store(data.comm, data.kStateIndex, data.kWorkerDone);
  };
}

const worker = new Worker(workerCode, {type: 'function'});
worker.postMessage({mem, comm, kMaxPages, kStateIndex, kWorkerDone});
assertEquals('started', worker.getMessage());

while (Atomics.load(comm, kStateIndex) == kWorkerRunning) {
  new WebAssembly.Instance(module, {mod: {mem}});
}
