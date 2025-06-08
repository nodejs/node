// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function PostSharedStruct() {
  print(arguments.callee.name);

  let builder = new WasmModuleBuilder();

  let struct = builder.addStruct([makeField(kWasmI32, true)], kNoSuperType,
                                  false, true);
  let producer_sig = makeSig([kWasmI32], [wasmRefType(struct)]);
  builder.addFunction("producer", producer_sig)
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructNew, struct])
    .exportFunc();
  let getter_sig = makeSig([wasmRefNullType(struct)], [kWasmI32]);
  builder.addFunction("getter", getter_sig)
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructGet, struct, 0])
    .exportFunc();

  let instance = builder.instantiate();

  let worker = new Worker(function() {
    onmessage = function({data:msg}) {
      d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

      let builder = new WasmModuleBuilder();

      let struct = builder.addStruct([makeField(kWasmI32, true)], kNoSuperType,
                                      false, true);
      let setter_sig = makeSig([wasmRefNullType(struct), kWasmI32], []);
      builder.addFunction("setter", setter_sig)
        .addBody([kExprLocalGet, 0, kExprLocalGet, 1,
                  kGCPrefix, kExprStructSet, struct, 0])
        .exportFunc();
      let instance = builder.instantiate();
      let value1 = 10;
      instance.exports.setter(msg.struct, value1);
      postMessage({struct: msg.struct});
      return;
    }
  }, {type: 'function'});

  let value0 = 5;
  let value1 = 10;
  let struct_obj_pre = instance.exports.producer(value0);
  assertEquals(instance.exports.getter(struct_obj_pre), value0);
  worker.postMessage({struct: struct_obj_pre});

  let struct_obj_post = worker.getMessage().struct;

  assertEquals(struct_obj_pre, struct_obj_post);
  assertEquals(instance.exports.getter(struct_obj_post), value1);
})();

// Trying to postMessage a non-shared struct should fail.
(function PostNonSharedStruct() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  let struct = builder.addStruct([makeField(kWasmI32, true)], kNoSuperType,
                                  false, false);
  let producer_sig = makeSig([kWasmI32], [wasmRefType(struct)]);
  builder.addFunction("producer", producer_sig)
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructNew, struct])
    .exportFunc();
  let getter_sig = makeSig([wasmRefNullType(struct)], [kWasmI32]);
  builder.addFunction("getter", getter_sig)
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructGet, struct, 0])
    .exportFunc();

  let instance = builder.instantiate();

  let worker = new Worker(function() {
    onmessage = function({data:msg}) {
      d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

      let builder = new WasmModuleBuilder();

      let struct = builder.addStruct([makeField(kWasmI32, true)], kNoSuperType,
                                      false, false);
      let setter_sig = makeSig([wasmRefNullType(struct), kWasmI32], []);
      builder.addFunction("setter", setter_sig)
        .addBody([kExprLocalGet, 0, kExprLocalGet, 1,
                  kGCPrefix, kExprStructSet, struct, 0])
        .exportFunc();
      let instance = builder.instantiate();
      let value1 = 10;
      instance.exports.setter(msg.struct, value1);
      postMessage({struct: msg.struct});
      return;
    }
  }, {type: 'function'});

  let value0 = 5;
  let struct_obj_pre = instance.exports.producer(value0);
  assertEquals(instance.exports.getter(struct_obj_pre), value0);
  assertThrows(() => worker.postMessage({struct: struct_obj_pre}), Error,
               "[object Object] could not be cloned.");
})();

(function PostSharedArray() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  let array = builder.addArray(kWasmI32, true, kNoSuperType, false, true);
  let producer_sig = makeSig([kWasmI32], [wasmRefType(array)]);
  builder.addFunction("producer", producer_sig)
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprArrayNewFixed, array, 1])
    .exportFunc();
  let getter_sig = makeSig([wasmRefNullType(array)], [kWasmI32]);
  builder.addFunction("getter", getter_sig)
    .addBody([kExprLocalGet, 0, kExprI32Const, 0,
              kGCPrefix, kExprArrayGet, array])
    .exportFunc();

  let instance = builder.instantiate();

  let worker = new Worker(function() {
    onmessage = function({data:msg}) {
      d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

      let builder = new WasmModuleBuilder();

      let array = builder.addArray(kWasmI32, true, kNoSuperType, false, true);
      let setter_sig = makeSig([wasmRefNullType(array), kWasmI32], []);
      builder.addFunction("setter", setter_sig)
        .addBody([kExprLocalGet, 0, kExprI32Const, 0, kExprLocalGet, 1,
                  kGCPrefix, kExprArraySet, array])
        .exportFunc();
      let instance = builder.instantiate();
      let value1 = 10;
      instance.exports.setter(msg.array, value1);
      postMessage({array: msg.array});
      return;
    }
  }, {type: 'function'});

  let value0 = 5;
  let value1 = 10;
  let array_obj_pre = instance.exports.producer(value0);
  assertEquals(instance.exports.getter(array_obj_pre), value0);
  worker.postMessage({array: array_obj_pre});

  let array_obj_post = worker.getMessage().array;

  assertEquals(array_obj_pre, array_obj_post);
  assertEquals(instance.exports.getter(array_obj_post), value1);
})();

// Trying to postMessage a non-shared array should fail.
(function PostNonSharedArray() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  let array = builder.addArray(kWasmI32, true, kNoSuperType, false, false);
  let producer_sig = makeSig([kWasmI32], [wasmRefType(array)]);
  builder.addFunction("producer", producer_sig)
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprArrayNewFixed, array, 1])
    .exportFunc();
  let getter_sig = makeSig([wasmRefNullType(array)], [kWasmI32]);
  builder.addFunction("getter", getter_sig)
    .addBody([kExprLocalGet, 0, kExprI32Const, 0,
              kGCPrefix, kExprArrayGet, array])
    .exportFunc();

  let instance = builder.instantiate();

  let worker = new Worker(function() {
    onmessage = function({data:msg}) {
      d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

      let builder = new WasmModuleBuilder();

      let array = builder.addArray(kWasmI32, true, kNoSuperType, false, false);
      let setter_sig = makeSig([wasmRefNullType(array), kWasmI32], []);
      builder.addFunction("setter", setter_sig)
        .addBody([kExprLocalGet, 0, kExprI32Const, 0, kExprLocalGet, 1,
                  kGCPrefix, kExprArraySet, array])
        .exportFunc();
      let instance = builder.instantiate();
      let value1 = 10;
      instance.exports.setter(msg.array, value1);
      postMessage({array: msg.array});
      return;
    }
  }, {type: 'function'});

  let value0 = 5;
  let value1 = 10;
  let array_obj_pre = instance.exports.producer(value0);
  assertEquals(instance.exports.getter(array_obj_pre), value0);
  assertThrows(() => worker.postMessage({array: array_obj_pre}), Error,
               "[object Object] could not be cloned.");
})();

(function PostSharedStructAllocatedInWorker() {
  print(arguments.callee.name);

  let builder = new WasmModuleBuilder();

  let struct = builder.addStruct([makeField(kWasmI32, true)], kNoSuperType,
                                  false, true);

  let getter_sig = makeSig([wasmRefNullType(struct)], [kWasmI32]);
  builder.addFunction("getter", getter_sig)
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructGet, struct, 0])
    .exportFunc();

  let instance = builder.instantiate();

  let worker = new Worker(function() {
    onmessage = function({data:msg}) {
      d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

      let builder = new WasmModuleBuilder();

      let struct = builder.addStruct([makeField(kWasmI32, true)], kNoSuperType,
                                      false, true);
      let producer_sig = makeSig([kWasmI32], [wasmRefType(struct)])
      builder.addFunction("producer", producer_sig)
        .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructNew, struct])
        .exportFunc();

      let instance = builder.instantiate();
      let value = 10;
      let struct_obj = instance.exports.producer(value);
      postMessage({struct: struct_obj});
      return;
    }
  }, {type: 'function'});

  let value = 10;

  worker.postMessage({});

  let struct_obj = worker.getMessage().struct;

  assertEquals(instance.exports.getter(struct_obj), value);
})();
