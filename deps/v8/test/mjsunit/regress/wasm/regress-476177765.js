// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const worker_fn = function({data: data}) {
  if (data.module) {
    this.instance =
        new WebAssembly.Instance(data.module, {m: {memory: data.mem}});
  } else {
    for (let i = 0; i < 10; ++i) {
      instance.exports.grow();
    }
    postMessage('done');
  }
};
const workers = [];
for (let i = 0; i < 4; i++) {
  let worker =
      new Worker('onmessage = ' + worker_fn.toString(), {type: 'string'});
  workers.push(worker);
}

const builder = new WasmModuleBuilder();
builder.addImportedMemory('m', 'memory', 1, 500, 'shared');
builder.addFunction('grow', kSig_i_v)
    .addBody([kExprI32Const, 1, kExprMemoryGrow, kMemoryZero])
    .exportFunc();
const module = builder.toModule();
const memory = new WebAssembly.Memory({initial: 1, maximum: 500, shared: true});

for (const worker of workers) {
  worker.postMessage({module: module, mem: memory});
}
for (const worker of workers) {
  worker.postMessage({});
}
