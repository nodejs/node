// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-enforce-bounds-checks

d8.file.execute('test/mjsunit/mjsunit.js');
d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const memory = new WebAssembly.Memory({initial: 1, maximum: 5, shared: true});

const builder = new WasmModuleBuilder();
builder.addImportedMemory('mod', 'mem', 1, 5, true);

// Function:
// Performs memory load at byte offset 65536 (page 2) without calling
// memory.size.
const body = [
  ...wasmI32Const(65536),
  kAtomicPrefix, kExprI32AtomicLoad, 2, 0,
];

builder.addFunction('load_page2_without_size', makeSig([], [kWasmI32]))
  .addBody(body)
  .exportFunc();

const module = new WebAssembly.Module(builder.toBuffer());

function workerCode() {
  onmessage = function({data: {module, memory}}) {
    try {
      const instance = new WebAssembly.Instance(module, {mod: {mem: memory}});
      postMessage({status: 'ready'});
      // Wait for main thread to grow memory and signal completion.
      const i32 = new Int32Array(memory.buffer);
      while (Atomics.load(i32, 0) !== 2) {}
      const val = instance.exports.load_page2_without_size();
      postMessage({status: 'done', val: val});
    } catch (e) {
      postMessage({status: 'error', error: String(e)});
    }
  };
}

const worker = new Worker(workerCode, {type: 'function'});
worker.postMessage({module: module, memory: memory});

const msg1 = worker.getMessage();
assertEquals('ready', msg1.status, msg1.error);

// Main thread grows memory
memory.grow(1);

const i32_grown = new Int32Array(memory.buffer);
// Write payload to byte offset 65536 (index 16384 in Int32Array)
Atomics.store(i32_grown, 16384, 0x123456);

// Release worker by setting memory[0] = 2
Atomics.store(i32_grown, 0, 2);

const msg2 = worker.getMessage();
assertEquals('done', msg2.status, msg2.error);
assertEquals(0x123456, msg2.val);

worker.terminate();
