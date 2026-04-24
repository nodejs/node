// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function TestSetGet() {
  print(arguments.callee.name);

  let builder = new WasmModuleBuilder();

  let struct = builder.addStruct(
    {fields: [makeField(kWasmWaitQueue, true), makeField(kWasmI32, true)],
     shared: true});

  let struct_type = wasmRefNullType(struct);

  builder.addFunction("make", makeSig([kWasmI32, kWasmI32], [struct_type]))
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1,
              kGCPrefix, kExprStructNew, struct])
    .exportFunc();

  builder.addFunction("get", makeSig([struct_type], [kWasmI32]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructGet, struct, 0])
    .exportFunc();

  builder.addFunction("set", makeSig([struct_type, kWasmI32], []))
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1,
              kGCPrefix, kExprStructSet, struct, 0])
    .exportFunc();

  builder.addFunction("atomic_get", makeSig([struct_type], [kWasmI32]))
    .addBody([
        kExprLocalGet, 0,
        kAtomicPrefix, kExprStructAtomicGet, kAtomicSeqCst, struct, 0])
    .exportFunc();

  builder.addFunction("atomic_set", makeSig([struct_type, kWasmI32], []))
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kAtomicPrefix, kExprStructAtomicSet, kAtomicSeqCst, struct, 0])
    .exportFunc();

  builder.addFunction("atomic_add",
                      makeSig([struct_type, kWasmI32], [kWasmI32]))
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1,
              kAtomicPrefix, kExprStructAtomicAdd, kAtomicSeqCst, struct, 0])
    .exportFunc();

  builder.addFunction("atomic_cmpxchg",
                      makeSig([struct_type, kWasmI32, kWasmI32], [kWasmI32]))
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprLocalGet, 2,
              kAtomicPrefix, kExprStructAtomicCompareExchange, kAtomicSeqCst,
              struct, 0])
    .exportFunc();

  builder.addFunction("get_i32", makeSig([struct_type], [kWasmI32]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructGet, struct, 1])
    .exportFunc();

  let wasm = builder.instantiate().exports;

  let value0 = 42;
  let i32_value = 10;

  let struct_obj = wasm.make(value0, i32_value);
  assertEquals(value0, wasm.get(struct_obj));
  // Make sure the waitqueue does not overwrite the next field.
  assertEquals(i32_value, wasm.get_i32(struct_obj));
  let value1 = -100;
  wasm.set(struct_obj, value1);
  assertEquals(value1, wasm.get(struct_obj));
  let value2 = 10;
  wasm.atomic_set(struct_obj, value2);
  assertEquals(value2, wasm.atomic_get(struct_obj));
  // Make sure setting the waitqueue's control value does not overwrite the next
  // field.
  assertEquals(i32_value, wasm.get_i32(struct_obj));

  let value3 = 5;
  assertEquals(value2, wasm.atomic_add(struct_obj, value3));
  let value4 = -1;
  assertEquals(value2 + value3,
               wasm.atomic_cmpxchg(struct_obj, value2 + value3, value4));
  assertEquals(value4, wasm.atomic_cmpxchg(struct_obj, value0, value0));  // nop
  assertEquals(value4, wasm.atomic_get(struct_obj));
})();

(function TestWaitNotify() {
  print(arguments.callee.name);

  let value0 = 42;
  let value1 = 10;

  let builder = new WasmModuleBuilder();

  let struct = builder.addStruct({fields: [makeField(kWasmWaitQueue, true)],
                                  shared: true});

  let struct_type = wasmRefNullType(struct);

  let global = builder.addGlobal(
      struct_type, true, false,
      [kExprI32Const, value0, kGCPrefix, kExprStructNew, struct]);

  builder.addExportOfKind("global", kExternalGlobal, global.index);

  builder.addFunction("make", makeSig([kWasmI32], [struct_type]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructNew, struct])
    .exportFunc();

  builder.addFunction(
      "notify", makeSig([struct_type, kWasmI32], [kWasmI32]))
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1,
              kAtomicPrefix, kExprStructNotify, struct, 0])
    .exportFunc();

  builder.addFunction(
      "wait", makeSig([struct_type, kWasmI32, kWasmI64], [kWasmI32]))
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprLocalGet, 2,
              kAtomicPrefix, kExprStructWait, struct, 0])
    .exportFunc();

  let module = builder.toModule();

  let instance = new WebAssembly.Instance(module, {});

  let struct_from_function = instance.exports.make(value0);
  let struct_global = instance.exports.global.value;

  function workerCode() {
    instance = undefined;
    onmessage = function({data:msg}) {
      if (!instance) {
        instance = new WebAssembly.Instance(msg.module, {});
        postMessage('instantiated');
      } else {
        try {
          let result = instance.exports.wait(...msg.arguments);
          print(`[worker] ${result}`);
          postMessage(result);
        } catch (e) {
          if (e instanceof WebAssembly.RuntimeError &&
              e.message.includes("dereferencing a null pointer")) {
            postMessage("dereferencing a null pointer");
          } else {
            postMessage("unknown exception");
          }
        }
      }
      return;
    }
  }

  let worker = new Worker(workerCode, {type: 'function'});

  worker.postMessage({module: module});
  assertEquals('instantiated', worker.getMessage());

  const k1ms = 1_000_000n;
  const k10s = 10_000_000_000n;

  worker.postMessage({arguments: [null, value1, k1ms]});
  assertEquals("dereferencing a null pointer", worker.getMessage());
  assertTraps(kTrapNullDereference, () => instance.exports.notify(null, 1));

  for (struct_obj of [struct_from_function, struct_global]) {
    worker.postMessage({arguments: [struct_obj, value1, k1ms]});
    assertEquals(kAtomicWaitNotEqual, worker.getMessage());

    worker.postMessage({arguments: [struct_obj, value0, k1ms]});
    assertEquals(kAtomicWaitTimedOut, worker.getMessage());

    worker.postMessage({arguments: [struct_obj, value0, k10s]});
    const started = performance.now();
    let notified;

    while ((notified = instance.exports.notify(struct_obj, 1)) === 0) {
      const now = performance.now();
      if (now - started > k10s) {
        throw new Error('Could not notify worker within 10s');
      }
    }
    assertEquals(1, notified);
    assertEquals(kAtomicWaitOk, worker.getMessage());
  }

  /* Test multiple workers. */
  let worker2 = new Worker(workerCode, {type: 'function'});

  worker2.postMessage({module: module});
  assertEquals('instantiated', worker2.getMessage());

  // 1. Notify them one by one.
  for (w of [worker, worker2]) {
    w.postMessage({arguments: [struct_obj, value0, k10s]});
  }
  let started = performance.now();
  let notified;
  while ((notified = instance.exports.notify(struct_obj, 1)) === 0) {
    const now = performance.now();
    if (now - started > k10s) {
      throw new Error('Could not notify worker within 10s');
    }
  }
  assertEquals(1, notified);
  while ((notified = instance.exports.notify(struct_obj, 1)) === 0) {
    const now = performance.now();
    if (now - started > k10s) {
      throw new Error('Could not notify worker within 10s');
    }
  }
  assertEquals(1, notified);
  assertEquals(kAtomicWaitOk, worker.getMessage());
  assertEquals(kAtomicWaitOk, worker2.getMessage());

  // 2. Notify them together.
  for (how_many_notified of [2, 10]) {
    for (w of [worker, worker2]) {
      w.postMessage({arguments: [struct_obj, value0, k10s]});
    }

    notified = 0;
    started = performance.now();
    while (notified < 2) {
      let current_notified = 0;
      while ((current_notified =
          instance.exports.notify(struct_obj, how_many_notified)) === 0) {
        const now = performance.now();
        if (now - started > k10s) {
          throw new Error('Could not notify worker within 10s');
        }
      }
      notified += current_notified;
    }
    assertEquals(2, notified);
    assertEquals(kAtomicWaitOk, worker.getMessage());
    assertEquals(kAtomicWaitOk, worker2.getMessage());
  }

  worker.terminate();
  worker2.terminate();
})();
