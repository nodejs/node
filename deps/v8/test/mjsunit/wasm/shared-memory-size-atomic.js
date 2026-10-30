// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/mjsunit.js');
d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const memory = new WebAssembly.Memory({initial: 1, maximum: 5, shared: true});

const builder = new WasmModuleBuilder();
builder.addImportedMemory('mod', 'mem', 1, 5, true);

// Function:
// 1. Sets memory[0] = 1 atomically to signal main thread.
// 2. Waits in Loop 1 for memory[0] == 2 (main thread signal).
// 3. Straight-line atomic load checks (NO loop, so no stack check interrupts).
// 4. Reads memory.size onto the stack (atomic seq_cst load).
// 5. Performs a memory load at byte offset 65536 (page 2).
// 6. Returns [size, page2_val].
const body = [
  ...wasmI32Const(0),
  ...wasmI32Const(1),
  kAtomicPrefix, kExprI32AtomicStore, 2, 0,

  // Loop 1: Wait for memory[0] == 2
  kExprLoop, kWasmVoid,
    ...wasmI32Const(0),
    kAtomicPrefix, kExprI32AtomicLoad, 2, 0,
    kExprI32Const, 2,
    kExprI32Ne,
    kExprBrIf, 0,
  kExprEnd,

  // Straight-line: read memory.size and perform memory load at byte
  // offset 65536.
  kExprMemorySize, 0,
  ...wasmI32Const(65536),
  kAtomicPrefix, kExprI32AtomicLoad, 2, 0,
];

builder.addFunction(
    'check_size_and_load_page2', makeSig([], [kWasmI32, kWasmI32]))
  .addBody(body)
  .exportFunc();

const module = new WebAssembly.Module(builder.toBuffer());

function workerCode() {
  onmessage = function({data: {module, memory}}) {
    try {
      const instance = new WebAssembly.Instance(module, {mod: {mem: memory}});
      postMessage({status: 'ready'});
      const results = instance.exports.check_size_and_load_page2();
      postMessage({status: 'done', size: results[0], page2_val: results[1]});
    } catch (e) {
      postMessage({status: 'error', error: String(e)});
    }
  };
}

const worker = new Worker(workerCode, {type: 'function'});
worker.postMessage({module: module, memory: memory});

const msg1 = worker.getMessage();
assertEquals('ready', msg1.status, msg1.error);

// Wait for worker to enter the Wasm function Loop 1
const i32 = new Int32Array(memory.buffer);
while (Atomics.load(i32, 0) !== 1) {}

// Main thread grows memory first while worker is waiting in Loop 1
memory.grow(1);

const i32_grown = new Int32Array(memory.buffer);
// Write magic payload to byte offset 65536 (index 16384 in Int32Array)
Atomics.store(i32_grown, 16384, 0x778899);

// Release worker from Loop 1 by setting memory[0] = 2
Atomics.store(i32_grown, 0, 2);

const msg2 = worker.getMessage();
assertEquals('done', msg2.status, msg2.error);
// Worker observed memory.size == 2 and successfully loaded 0x778899 from
// page 2 (byte offset 65536)
assertEquals(2, msg2.size);
assertEquals(0x778899, msg2.page2_val);

worker.terminate();
