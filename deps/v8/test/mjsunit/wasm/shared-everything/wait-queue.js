// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-shared

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function TestInvalidAtomicConstantExpression() {
  // Slightly unrelated, but since we changed how atomic non-constant
  // instructions are handled, we are testing it here.
  print(arguments.callee.name);

  let builder = new WasmModuleBuilder();

  builder.addGlobal(kWasmI32, true, false, [kAtomicPrefix, kExprPause]);

  assertThrows(() => builder.instantiate(), WebAssembly.CompileError,
               /opcode pause is not allowed in constant expressions/);
})();

(function TestTypeTestAndCast() {
  print(arguments.callee.name);

  let builder = new WasmModuleBuilder();

  builder.addFunction("test", kSig_i_v)
    .addBody([kAtomicPrefix, kExprWaitqueueNew,
              kGCPrefix, kExprRefTest, kWasmSharedTypeForm, kWaitqueueRefCode])
    .exportFunc();

  builder.addFunction("test_to_null", kSig_i_v)
    .addBody([kAtomicPrefix, kExprWaitqueueNew,
              kGCPrefix, kExprRefTestNull,
              kWasmSharedTypeForm, kWaitqueueRefCode])
    .exportFunc();

  builder.addFunction("test_null", kSig_i_v)
    .addBody([kExprRefNull, kWasmSharedTypeForm, kWaitqueueRefCode,
              kGCPrefix, kExprRefTest, kWasmSharedTypeForm, kWaitqueueRefCode])
    .exportFunc();

  builder.addFunction("test_null_to_null", kSig_i_v)
    .addBody([kExprRefNull, kWasmSharedTypeForm, kWaitqueueRefCode,
              kGCPrefix, kExprRefTestNull,
              kWasmSharedTypeForm, kWaitqueueRefCode])
    .exportFunc();

  builder.addFunction("assert", kSig_v_v)
    .addLocals(wasmRefNullType(kWasmWaitqueueRef).shared(), 1)
    .addBody([kAtomicPrefix, kExprWaitqueueNew, kExprLocalSet, 0,
              kExprLocalGet, 0, kExprRefAsNonNull, kExprDrop])
    .exportFunc();

  builder.addFunction("assert_null", kSig_v_v)
    .addBody([kExprRefNull, kWasmSharedTypeForm, kWaitqueueRefCode,
              kExprRefAsNonNull, kExprDrop])
    .exportFunc();

  builder.addFunction("cast", kSig_v_v)
    .addBody([kAtomicPrefix, kExprWaitqueueNew,
              kGCPrefix, kExprRefCast, kWasmSharedTypeForm, kWaitqueueRefCode,
              kExprDrop])
    .exportFunc();

  builder.addFunction("cast_to_null", kSig_v_v)
    .addBody([kAtomicPrefix, kExprWaitqueueNew,
              kGCPrefix, kExprRefCastNull,
              kWasmSharedTypeForm, kWaitqueueRefCode,
              kExprDrop])
    .exportFunc();

  builder.addFunction("cast_null", kSig_v_v)
    .addBody([kExprRefNull, kWasmSharedTypeForm, kWaitqueueRefCode,
              kGCPrefix, kExprRefCast, kWasmSharedTypeForm, kWaitqueueRefCode,
              kExprDrop])
    .exportFunc();

  builder.addFunction("cast_null_to_null", kSig_v_v)
    .addBody([kExprRefNull, kWasmSharedTypeForm, kWaitqueueRefCode,
              kGCPrefix, kExprRefCastNull,
              kWasmSharedTypeForm, kWaitqueueRefCode,
              kExprDrop])
    .exportFunc();

  let wasm = builder.instantiate().exports;

  assertEquals(1, wasm.test());
  assertEquals(1, wasm.test_to_null());
  assertEquals(0, wasm.test_null());
  assertEquals(1, wasm.test_null_to_null());
  wasm.assert();
  assertTraps(kTrapNullDereference, () => wasm.assert_null());
  wasm.cast();
  wasm.cast_to_null();
  assertTraps(kTrapIllegalCast, () => wasm.cast_null());
  wasm.cast_null_to_null();
})();

function typeToName(type) {
  if (type == kWasmI32) return "i32";
  else if (type == kWasmI64) return "i64";
  else return "eqref";
}

function TestWaitNotifyStruct(type, const_op, value0, value1) {
  print(`${arguments.callee.name} ${typeToName(type)}`);

  let builder = new WasmModuleBuilder();

  let struct = builder.addStruct(
      {fields: [makeField(type, true),
                makeField(wasmRefNullType(kWasmWaitqueueRef).shared(), true)],
       shared: true});

  let struct_type = wasmRefNullType(struct);

  let global = builder.addGlobal(
      struct_type, true, false,
      [...const_op(value0), kAtomicPrefix, kExprWaitqueueNew,
       kGCPrefix, kExprStructNew, struct]);

  builder.addExportOfKind("global", kExternalGlobal, global.index);

  builder.addFunction("make", makeSig([type], [struct_type]))
    .addBody([kExprLocalGet, 0, kAtomicPrefix, kExprWaitqueueNew,
              kGCPrefix, kExprStructNew, struct])
    .exportFunc();

  builder.addFunction(
      "notify", makeSig([struct_type, kWasmI32], [kWasmI32]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructGet, struct, 1,
              kExprLocalGet, 1,
              kAtomicPrefix, kExprWaitqueueNotify])
    .exportFunc();

  builder.addFunction("notify_null", kSig_i_i)
    .addBody([kExprRefNull, kWasmSharedTypeForm, kWaitqueueRefCode,
              kExprLocalGet, 0, kAtomicPrefix, kExprWaitqueueNotify])
    .exportFunc();

  builder.addFunction(
      "wait", makeSig([struct_type, type, kWasmI64], [kWasmI32]))
    .addBody([kExprLocalGet, 0,
              kExprLocalGet, 0, kGCPrefix, kExprStructGet, struct, 1,
              kExprLocalGet, 1, kExprLocalGet, 2,
              kAtomicPrefix, kExprStructWait, struct, 0])
    .exportFunc();

  builder.addFunction(
      "wait_null_struct", makeSig([type, kWasmI64], [kWasmI32]))
    .addBody([kExprRefNull, struct, kAtomicPrefix, kExprWaitqueueNew,
              kExprLocalGet, 0, kExprLocalGet, 1,
              kAtomicPrefix, kExprStructWait, struct, 0])
    .exportFunc();

  builder.addFunction(
      "wait_null_waitqueue",
      makeSig([struct_type, type, kWasmI64], [kWasmI32]))
    .addBody([kExprLocalGet, 0,
              kExprRefNull, kWasmSharedTypeForm, kWaitqueueRefCode,
              kExprLocalGet, 1, kExprLocalGet, 2,
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
          let result = msg.kind == "normal" ?
                          instance.exports.wait(...msg.arguments)
                     : msg.kind == "null_struct" ?
                          instance.exports.wait_null_struct(...msg.arguments)
                     : instance.exports.wait_null_waitqueue(...msg.arguments);
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

  worker.postMessage({kind: "null_struct", arguments: [value1, k1ms]});
  assertEquals("dereferencing a null pointer", worker.getMessage());
  worker.postMessage({kind: "null_waitqueue",
                      arguments: [struct_from_function, value1, k1ms]});
  assertEquals("dereferencing a null pointer", worker.getMessage());
  assertTraps(kTrapNullDereference, () => instance.exports.notify_null(1));

  for (struct_obj of [struct_from_function, struct_global]) {
    worker.postMessage({kind: "normal", arguments: [struct_obj, value1, k1ms]});
    assertEquals(kAtomicWaitNotEqual, worker.getMessage());

    worker.postMessage({kind: "normal", arguments: [struct_obj, value0, k1ms]});
    assertEquals(kAtomicWaitTimedOut, worker.getMessage());

    worker.postMessage({kind: "normal", arguments: [struct_obj, value0, k10s]});
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
    w.postMessage({kind: "normal", arguments: [struct_obj, value0, k10s]});
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
      w.postMessage({kind: "normal", arguments: [struct_obj, value0, k10s]});
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
}

TestWaitNotifyStruct(kWasmI32, wasmI32Const, 42, 10);
TestWaitNotifyStruct(kWasmI64, wasmI64Const, (1n << 40n) + 42n,
                     (1n << 40n) + 10n);
TestWaitNotifyStruct(
    wasmRefNullType(kWasmEqRef).shared(),
    v => [...wasmI32Const(v), kGCPrefix, kExprRefI31Shared], 42, 10);

function TestWaitNotifyArray(type, const_op, value0, value1) {
  print(`${arguments.callee.name} ${typeToName(type)}`);

  let builder = new WasmModuleBuilder();

  let array = builder.addArray(type, {shared: true});
  let array_type = wasmRefNullType(array);

  let struct = builder.addStruct(
      {fields: [makeField(array_type, true),
                makeField(wasmRefNullType(kWasmWaitqueueRef).shared(), true)],
       shared: true});

  let struct_type = wasmRefNullType(struct);

  let global = builder.addGlobal(
      struct_type, true, false,
      [...const_op(value1), ...const_op(value0),
       kGCPrefix, kExprArrayNewFixed, array, 2,
       kAtomicPrefix, kExprWaitqueueNew,
       kGCPrefix, kExprStructNew, struct]);

  builder.addExportOfKind("global", kExternalGlobal, global.index);

  builder.addFunction("make", makeSig([type], [struct_type]))
    .addBody([...const_op(value1), kExprLocalGet, 0,
              kGCPrefix, kExprArrayNewFixed, array, 2,
              kAtomicPrefix, kExprWaitqueueNew,
              kGCPrefix, kExprStructNew, struct])
    .exportFunc();

  builder.addFunction(
      "notify", makeSig([struct_type, kWasmI32], [kWasmI32]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructGet, struct, 1,
              kExprLocalGet, 1,
              kAtomicPrefix, kExprWaitqueueNotify])
    .exportFunc();

  builder.addFunction(
      "wait", makeSig([struct_type, kWasmI32, type, kWasmI64], [kWasmI32]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructGet, struct, 0,
              kExprLocalGet, 0, kGCPrefix, kExprStructGet, struct, 1,
              kExprLocalGet, 1, kExprLocalGet, 2, kExprLocalGet, 3,
              kAtomicPrefix, kExprArrayWait, array])
    .exportFunc();

  builder.addFunction(
      "wait_null_array", makeSig([kWasmI32, type, kWasmI64], [kWasmI32]))
    .addBody([kExprRefNull, array, kAtomicPrefix, kExprWaitqueueNew,
              kExprLocalGet, 0, kExprLocalGet, 1, kExprLocalGet, 2,
              kAtomicPrefix, kExprArrayWait, array])
    .exportFunc();

  builder.addFunction(
      "wait_null_waitqueue",
      makeSig([struct_type, kWasmI32, type, kWasmI64], [kWasmI32]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructGet, struct, 0,
              kExprRefNull, kWasmSharedTypeForm, kWaitqueueRefCode,
              kExprLocalGet, 1, kExprLocalGet, 2, kExprLocalGet, 3,
              kAtomicPrefix, kExprArrayWait, array])
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
          let result = msg.kind == "normal" ?
                          instance.exports.wait(...msg.arguments)
                     : msg.kind == "null_array" ?
                          instance.exports.wait_null_array(...msg.arguments)
                     : instance.exports.wait_null_waitqueue(...msg.arguments);
          print(`[worker] ${result}`);
          postMessage(result);
        } catch (e) {
          if (e instanceof WebAssembly.RuntimeError &&
              e.message.includes("dereferencing a null pointer")) {
            postMessage("dereferencing a null pointer");
          } else if (e instanceof WebAssembly.RuntimeError &&
                     e.message.includes("array element access out of bounds")) {
            postMessage("array element access out of bounds");
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

  worker.postMessage({kind: "null_array", arguments: [1, value1, k1ms]});
  assertEquals("dereferencing a null pointer", worker.getMessage());
  worker.postMessage({kind: "null_waitqueue",
                      arguments: [struct_from_function, 1, value1, k1ms]});
  assertEquals("dereferencing a null pointer", worker.getMessage());
  worker.postMessage({kind: "normal",
                      arguments: [struct_from_function, 2, value1, k1ms]});
  assertEquals("array element access out of bounds", worker.getMessage());

  for (struct_obj of [struct_from_function, struct_global]) {
    worker.postMessage({kind: "normal",
                        arguments: [struct_obj, 1, value1, k1ms]});
    assertEquals(kAtomicWaitNotEqual, worker.getMessage());

    worker.postMessage({kind: "normal",
                        arguments: [struct_obj, 1, value0, k1ms]});
    assertEquals(kAtomicWaitTimedOut, worker.getMessage());

    worker.postMessage({kind: "normal",
                        arguments: [struct_obj, 1, value0, k10s]});
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

  worker.terminate();
}

TestWaitNotifyArray(kWasmI32, wasmI32Const, 42, 10);
TestWaitNotifyArray(
    kWasmI64, wasmI64Const, (1n << 40n) + 42n, (1n << 40n) + 10n);
TestWaitNotifyArray(
    wasmRefNullType(kWasmEqRef).shared(),
    v => [...wasmI32Const(v), kGCPrefix, kExprRefI31Shared], 42, 10);
