// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-sharedarraybuffer
// Flags: --experimental-wasm-threads

'use strict';

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

function WasmAtomicWakeFunction(memory, offset, index, num) {
  let builder = new WasmModuleBuilder();
  builder.addImportedMemory("m", "memory", 0, 20, "shared");
  builder.addFunction("main", kSig_i_ii)
    .addBody([
      kExprGetLocal, 0,
      kExprGetLocal, 1,
      kAtomicPrefix,
      kExprAtomicWake, /* alignment */ 0, offset])
    .exportAs("main");

  // Instantiate module, get function exports
  let module = new WebAssembly.Module(builder.toBuffer());
  let instance = new WebAssembly.Instance(module, {m: {memory}});
  return instance.exports.main(index, num);
}

function WasmI32AtomicWaitFunction(memory, offset, index, val, timeout) {
  let builder = new WasmModuleBuilder();
  builder.addImportedMemory("m", "memory", 0, 20, "shared");
  builder.addFunction("main",
    makeSig([kWasmI32, kWasmI32, kWasmF64], [kWasmI32]))
    .addBody([
      kExprGetLocal, 0,
      kExprGetLocal, 1,
      kExprGetLocal, 2,
      kExprI64SConvertF64,
      kAtomicPrefix,
      kExprI32AtomicWait, /* alignment */ 0, offset])
      .exportAs("main");

  // Instantiate module, get function exports
  let module = new WebAssembly.Module(builder.toBuffer());
  let instance = new WebAssembly.Instance(module, {m: {memory}});
  return instance.exports.main(index, val, timeout);
}

(function TestInvalidIndex() {
  let memory = new WebAssembly.Memory({initial: 1, maximum: 1, shared: true});

  // Valid indexes are 0-65535 (1 page).
  [-2, 65536, 0xffffffff].forEach(function(invalidIndex) {
    assertThrows(function() {
      WasmAtomicWakeFunction(memory, 0, invalidIndex, -1);
    }, Error);
    assertThrows(function() {
      WasmI32AtomicWaitFunction(memory, 0, invalidIndex, 0, -1);
    }, Error);
    assertThrows(function() {
      WasmAtomicWakeFunction(memory, invalidIndex, 0, -1);
    }, Error);
    assertThrows(function() {
      WasmI32AtomicWaitFunction(memory, invalidIndex, 0, 0, -1);
    }, Error);
    assertThrows(function() {
      WasmAtomicWakeFunction(memory, invalidIndex/2, invalidIndex/2, -1);
    }, Error);
    assertThrows(function() {
      WasmI32AtomicWaitFunction(memory, invalidIndex/2, invalidIndex/2, 0, -1);
    }, Error);
  });
})();

(function TestWaitTimeout() {
  let memory = new WebAssembly.Memory({initial: 1, maximum: 1, shared: true});
  var waitMs = 100;
  var startTime = new Date();
  assertEquals(2, WasmI32AtomicWaitFunction(memory, 0, 0, 0, waitMs*1000000));
  var endTime = new Date();
  assertTrue(endTime - startTime >= waitMs);
})();

(function TestWaitNotEqual() {
  let memory = new WebAssembly.Memory({initial: 1, maximum: 1, shared: true});
  assertEquals(1, WasmI32AtomicWaitFunction(memory, 0, 0, 42, -1));

  assertEquals(2, WasmI32AtomicWaitFunction(memory, 0, 0, 0, 0));

  let i32a = new Int32Array(memory.buffer);
  i32a[0] = 1;
  assertEquals(1, WasmI32AtomicWaitFunction(memory, 0, 0, 0, -1));
})();

(function TestWakeCounts() {
  let memory = new WebAssembly.Memory({initial: 1, maximum: 1, shared: true});

  [-1, 0, 4, 100, 0xffffffff].forEach(function(count) {
    WasmAtomicWakeFunction(memory, 0, 0, count);
  });
})();

//// WORKER ONLY TESTS

if (this.Worker) {
  let wasm_wake_adapter = (memory, offset, index, count) => {
    return WasmAtomicWakeFunction(memory, offset, index, count);
  };

  let js_wake_adapter = (memory, offset, index, count) => {
    let i32a = new Int32Array(memory.buffer, offset);
    return Atomics.wake(i32a, index>>>2, count);
  };

  // Wait adapter strings that can be passed as a parameter to TestWaitWake to generate
  // custom worker script
  let js_wait_adapter = `(memory, offset, index, val) => {
    let i32a = new Int32Array(memory.buffer, offset);
    let res = Atomics.wait(i32a, index>>>2, val);
    if (res == "ok") return 0;
    if (res == "not-equal") return 1;
    return 2;
  }`

  let wasm_wait_adapter = `(memory, offset, index, val) => {
    load("test/mjsunit/wasm/wasm-constants.js");
    load("test/mjsunit/wasm/wasm-module-builder.js");
    ${WasmI32AtomicWaitFunction.toString()}
    return WasmI32AtomicWaitFunction(memory, offset, index, val, -1);
  }`

  let TestWaitWake = function(wait_adapter, wake_adapter, num_workers, num_workers_wake) {
    let memory = new WebAssembly.Memory({initial: 1, maximum: 1, shared: true});
    let i32a = new Int32Array(memory.buffer);
    // SAB values:
    // memory[id*4], where id in range [0, num_workers]:
    //   0 => Worker |id| is still waiting on the futex
    //   1 => Worker |id| is not waiting on futex, but has not be reaped by the
    //        main thread.
    //   2 => Worker |id| has been reaped.
    //
    // memory[num_workers*4]:
    //   always 0. Each worker is waiting on this index.

    let workerScript =
      `onmessage = function(msg) {
         let id = msg.id;
         let memory = msg.memory;
         let i32a = new Int32Array(memory.buffer);
         let wait_adapter = eval(msg.wait_adapter);
         let result = wait_adapter(memory, 0, 4*${num_workers}, 0);
         // Set i32a[id] to 1 to notify the main thread which workers were
         // woken up.
         Atomics.store(i32a, id, 1);
         postMessage(result);
       };`;

    let workers = [];
    for (let id = 0; id < num_workers; id++) {
      workers[id] = new Worker(workerScript, {type: 'string'});
      workers[id].postMessage({id, memory, wait_adapter});
    }

    // Spin until all workers are waiting on the futex.
    while (%AtomicsNumWaitersForTesting(i32a, num_workers) != num_workers) {}

    if (num_workers_wake < num_workers) {
      assertEquals(num_workers_wake, wake_adapter(memory, 0, 4*num_workers, num_workers_wake));
    } else {
      assertEquals(num_workers, wake_adapter(memory, 0, 4*num_workers, num_workers_wake));
      num_workers_wake = num_workers;
    }

    let wokenCount = 0;
    while (wokenCount < num_workers_wake) {
      for (let id = 0; id < num_workers; id++) {
        // Look for workers that have not yet been reaped. Set i32a[id] to 2
        // when they've been processed so we don't look at them again.
        if (Atomics.compareExchange(i32a, id, 1, 2) == 1) {
          assertEquals(0, workers[id].getMessage());
          workers[id].terminate();
          wokenCount++;
        }
      }
    }

    assertEquals(num_workers - num_workers_wake,
                 %AtomicsNumWaitersForTesting(i32a, num_workers));

    // Finally wake and kill all workers.
    wake_adapter(memory, 0, 4*num_workers, num_workers)
    for (let id = 0; id < num_workers; id++) {
      workers[id].terminate();
    }
  };

  TestWaitWake(js_wait_adapter, wasm_wake_adapter, 1, 1);
  TestWaitWake(js_wait_adapter, wasm_wake_adapter, 4, 4);
  TestWaitWake(js_wait_adapter, wasm_wake_adapter, 4, 3);
  TestWaitWake(js_wait_adapter, wasm_wake_adapter, 3, 4);

  TestWaitWake(wasm_wait_adapter, wasm_wake_adapter, 1, 1);
  TestWaitWake(wasm_wait_adapter, wasm_wake_adapter, 4, 4);
  TestWaitWake(wasm_wait_adapter, wasm_wake_adapter, 4, 3);
  TestWaitWake(wasm_wait_adapter, wasm_wake_adapter, 3, 4);

  TestWaitWake(wasm_wait_adapter, js_wake_adapter, 1, 1);
  TestWaitWake(wasm_wait_adapter, js_wake_adapter, 4, 4);
  TestWaitWake(wasm_wait_adapter, js_wake_adapter, 4, 3);
  TestWaitWake(wasm_wait_adapter, js_wake_adapter, 3, 4);
}
