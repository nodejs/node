// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-sharedarraybuffer
// Flags: --experimental-wasm-threads

'use strict';

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

function WasmAtomicWake(memory, offset, index, num) {
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

function WasmI32AtomicWait(memory, offset, index, val, timeout) {
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

function WasmI64AtomicWait(memory, offset, index, val_low,
                                   val_high, timeout) {
  let builder = new WasmModuleBuilder();
  builder.addImportedMemory("m", "memory", 0, 20, "shared");
  // Wrapper for I64AtomicWait that takes two I32 values and combines to into
  // I64 for the instruction parameter.
  builder.addFunction("main",
    makeSig([kWasmI32, kWasmI32, kWasmI32, kWasmF64], [kWasmI32]))
    .addLocals({i64_count: 1}) // local that is passed as value param to wait
    .addBody([
      kExprGetLocal, 1,
      kExprI64UConvertI32,
      kExprI64Const, 32,
      kExprI64Shl,
      kExprGetLocal, 2,
      kExprI64UConvertI32,
      kExprI64Ior,
      kExprSetLocal, 4, // Store the created I64 value in local
      kExprGetLocal, 0,
      kExprGetLocal, 4,
      kExprGetLocal, 3,
      kExprI64SConvertF64,
      kAtomicPrefix,
      kExprI64AtomicWait, /* alignment */ 0, offset])
      .exportAs("main");

  // Instantiate module, get function exports
  let module = new WebAssembly.Module(builder.toBuffer());
  let instance = new WebAssembly.Instance(module, {m: {memory}});
  return instance.exports.main(index, val_high, val_low, timeout);
}

(function TestInvalidIndex() {
  let memory = new WebAssembly.Memory({initial: 1, maximum: 1, shared: true});

  // Valid indexes are 0-65535 (1 page).
  [-2, 65536, 0xffffffff].forEach(function(invalidIndex) {
    assertThrows(function() {
      WasmAtomicWake(memory, 0, invalidIndex, -1);
    }, Error);
    assertThrows(function() {
      WasmI32AtomicWait(memory, 0, invalidIndex, 0, -1);
    }, Error);
    assertThrows(function() {
      WasmI64AtomicWait(memory, 0, invalidIndex, 0, 0, -1);
    }, Error);
    assertThrows(function() {
      WasmAtomicWake(memory, invalidIndex, 0, -1);
    }, Error);
    assertThrows(function() {
      WasmI32AtomicWait(memory, invalidIndex, 0, 0, -1);
    }, Error);
    assertThrows(function() {
      WasmI64AtomicWait(memory, invalidIndex, 0, 0, 0, -1);
    }, Error);
    assertThrows(function() {
      WasmAtomicWake(memory, invalidIndex/2, invalidIndex/2, -1);
    }, Error);
    assertThrows(function() {
      WasmI32AtomicWait(memory, invalidIndex/2, invalidIndex/2, 0, -1);
    }, Error);
    assertThrows(function() {
      WasmI64AtomicWait(memory, invalidIndex/2, invalidIndex/2, 0, 0, -1);
    }, Error);
  });
})();

(function TestI32WaitTimeout() {
  let memory = new WebAssembly.Memory({initial: 1, maximum: 1, shared: true});
  var waitMs = 100;
  var startTime = new Date();
  assertEquals(2, WasmI32AtomicWait(memory, 0, 0, 0, waitMs*1000000));
  var endTime = new Date();
  assertTrue(endTime - startTime >= waitMs);
})();

(function TestI64WaitTimeout() {
  let memory = new WebAssembly.Memory({initial: 1, maximum: 1, shared: true});
  var waitMs = 100;
  var startTime = new Date();
  assertEquals(2, WasmI64AtomicWait(memory, 0, 0, 0, 0, waitMs*1000000));
  var endTime = new Date();
  assertTrue(endTime - startTime >= waitMs);
})();

(function TestI32WaitNotEqual() {
  let memory = new WebAssembly.Memory({initial: 1, maximum: 1, shared: true});
  assertEquals(1, WasmI32AtomicWait(memory, 0, 0, 42, -1));

  assertEquals(2, WasmI32AtomicWait(memory, 0, 0, 0, 0));

  let i32a = new Int32Array(memory.buffer);
  i32a[0] = 1;
  assertEquals(1, WasmI32AtomicWait(memory, 0, 0, 0, -1));
  assertEquals(2, WasmI32AtomicWait(memory, 0, 0, 1, 0));
})();

(function TestI64WaitNotEqual() {
  let memory = new WebAssembly.Memory({initial: 1, maximum: 1, shared: true});
  assertEquals(1, WasmI64AtomicWait(memory, 0, 0, 42, 0, -1));

  assertEquals(2, WasmI64AtomicWait(memory, 0, 0, 0, 0, 0));

  let i32a = new Int32Array(memory.buffer);
  i32a[0] = 1;
  i32a[1] = 2;
  assertEquals(1, WasmI64AtomicWait(memory, 0, 0, 0, 0, -1));
  assertEquals(2, WasmI64AtomicWait(memory, 0, 0, 1, 2, 0));
})();

(function TestWakeCounts() {
  let memory = new WebAssembly.Memory({initial: 1, maximum: 1, shared: true});

  [-1, 0, 4, 100, 0xffffffff].forEach(function(count) {
    WasmAtomicWake(memory, 0, 0, count);
  });
})();

//// WORKER ONLY TESTS

if (this.Worker) {

  // This test creates 4 workers that wait on consecutive (8 byte separated to
  // satisfy alignments for all kinds of wait) memory locations to test various
  // wait/wake combinations. For each combination, each thread waits 3 times
  // expecting all 4 threads to be woken with wake(4) in first iteration, all 4
  // to be woken with wake(5) in second iteration and, 3 and 1 to be woken in
  // third iteration.

  let memory = new WebAssembly.Memory({initial: 1, maximum: 1, shared: true});
  let i32a = new Int32Array(memory.buffer);
  const numWorkers = 4;

  let workerScript = `onmessage = function(msg) {
    load("test/mjsunit/wasm/wasm-constants.js");
    load("test/mjsunit/wasm/wasm-module-builder.js");
    ${WasmI32AtomicWait.toString()}
    ${WasmI64AtomicWait.toString()}
    let id = msg.id;
    let memory = msg.memory;
    let i32a = new Int32Array(memory.buffer);
    // indices are right shifted by 2 for Atomics.wait to convert them to index
    // for Int32Array
    // for wasm-wake numWorkers threads
    let result = Atomics.wait(i32a, 0>>>2, 0);
    postMessage(result);
    // for wasm-wake numWorkers + 1 threads
    result = Atomics.wait(i32a, 8>>>2, 0);
    postMessage(result);
    // for wasm-wake numWorkers - 1 threads
    result = Atomics.wait(i32a, 16>>>2, 0);
    postMessage(result);
    // for js-wake numWorkers threads
    result = WasmI32AtomicWait(memory, 0, 24, 0, -1);
    postMessage(result);
    // for js-wake numWorkers + 1 threads
    result = WasmI32AtomicWait(memory, 0, 32, 0, -1);
    postMessage(result);
    // for js-wake numWorkers - 1 threads
    result = WasmI32AtomicWait(memory, 0, 40, 0, -1);
    postMessage(result);
    // for wasm-wake numWorkers threads
    result = WasmI32AtomicWait(memory, 0, 48, 0, -1);
    postMessage(result);
    // for wasm-wake numWorkers + 1 threads
    result = WasmI32AtomicWait(memory, 0, 56, 0, -1);
    postMessage(result);
    // for wasm-wake numWorkers - 1 threads
    result = WasmI32AtomicWait(memory, 0, 64, 0, -1);
    postMessage(result);
    // for js-wake numWorkers threads
    result = WasmI64AtomicWait(memory, 0, 72, 0, 0, -1);
    postMessage(result);
    // for js-wake numWorkers + 1 threads
    result = WasmI64AtomicWait(memory, 0, 80, 0, 0, -1);
    postMessage(result);
    // for js-wake numWorkers - 1 threads
    result = WasmI64AtomicWait(memory, 0, 88, 0, 0, -1);
    postMessage(result);
    // for wasm-wake numWorkers threads
    result = WasmI64AtomicWait(memory, 0, 96, 0, 0, -1);
    postMessage(result);
    // for wasm-wake numWorkers + 1 threads
    result = WasmI64AtomicWait(memory, 0, 104, 0, 0, -1);
    postMessage(result);
    // for wasm-wake numWorkers - 1 threads
    result = WasmI64AtomicWait(memory, 0, 112, 0, 0, -1);
    postMessage(result);
  };`;

  let waitForAllWorkers = function(index) {
    // index is right shifted by 2 to convert to index in Int32Array
    while (%AtomicsNumWaitersForTesting(i32a, index>>>2) != numWorkers) {}
  }

  let jsWakeCheck = function(index, num, workers, msg) {
    waitForAllWorkers(index);
    let indexJs = index>>>2; // convert to index in Int32Array
    if (num >= numWorkers) {
      // if numWorkers or more is passed to wake, numWorkers workers should be
      // woken.
      assertEquals(numWorkers, Atomics.wake(i32a, indexJs, num));
    } else {
      // if num < numWorkers is passed to wake, num workers should be woken.
      // Then the remaining workers are woken for the next part
      assertEquals(num, Atomics.wake(i32a, indexJs, num));
      assertEquals(numWorkers-num, Atomics.wake(i32a, indexJs, numWorkers));
    }
    for (let id = 0; id < numWorkers; id++) {
      assertEquals(msg, workers[id].getMessage());
    }
  };

  let wasmWakeCheck = function(index, num, workers, msg) {
    waitForAllWorkers(index);
    if (num >= numWorkers) {
      // if numWorkers or more is passed to wake, numWorkers workers should be
      // woken.
      assertEquals(numWorkers, WasmAtomicWake(memory, 0, index, num));
    } else {
      // if num < numWorkers is passed to wake, num workers should be woken.
      // Then the remaining workers are woken for the next part
      assertEquals(num, WasmAtomicWake(memory, 0, index, num));
      assertEquals(numWorkers-num,
                   WasmAtomicWake(memory, 0, index, numWorkers));
    }
    for (let id = 0; id < numWorkers; id++) {
      assertEquals(msg, workers[id].getMessage());
    }
  };

  let workers = [];
  for (let id = 0; id < numWorkers; id++) {
    workers[id] = new Worker(workerScript, {type: 'string'});
    workers[id].postMessage({id, memory});
  }

  wasmWakeCheck(0, numWorkers, workers, "ok");
  wasmWakeCheck(8, numWorkers + 1, workers, "ok");
  wasmWakeCheck(16, numWorkers - 1, workers, "ok");

  jsWakeCheck(24, numWorkers, workers, 0);
  jsWakeCheck(32, numWorkers + 1, workers, 0);
  jsWakeCheck(40, numWorkers - 1, workers, 0);

  wasmWakeCheck(48, numWorkers, workers, 0);
  wasmWakeCheck(56, numWorkers + 1, workers, 0);
  wasmWakeCheck(64, numWorkers - 1, workers, 0);

  jsWakeCheck(72, numWorkers, workers, 0);
  jsWakeCheck(80, numWorkers + 1, workers, 0);
  jsWakeCheck(88, numWorkers - 1, workers, 0);

  wasmWakeCheck(96, numWorkers, workers, 0);
  wasmWakeCheck(104, numWorkers + 1, workers, 0);
  wasmWakeCheck(112, numWorkers - 1, workers, 0);

  for (let id = 0; id < numWorkers; id++) {
    workers[id].terminate();
  }
}
