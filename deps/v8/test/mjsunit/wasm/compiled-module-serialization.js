// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --allow-natives-syntax --expose-gc

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

(function SerializeAndDeserializeModule() {
  var builder = new WasmModuleBuilder();
  builder.addMemory(1,1, true);
  var kSig_v_i = makeSig([kAstI32], []);
  var signature = builder.addType(kSig_v_i);
  builder.addImport("some_value", kSig_i);
  builder.addImport("writer", signature);

  builder.addFunction("main", kSig_i_i)
    .addBody([
      kExprGetLocal, 0,
      kExprI32LoadMem, 0, 0,
      kExprI32Const, 1,
      kExprCallIndirect, signature,
      kExprGetLocal,0,
      kExprI32LoadMem,0, 0,
      kExprCallFunction, 0,
      kExprI32Add
    ]).exportFunc();

  // writer(mem[i]);
  // return mem[i] + some_value();
  builder.addFunction("_wrap_writer", signature)
    .addBody([
      kExprGetLocal, 0,
      kExprCallFunction, 1]);
  builder.appendToTable([2, 3]);


  var module = new WebAssembly.Module(builder.toBuffer());
  var buff = %SerializeWasmModule(module);
  module = null;
  gc();
  module = %DeserializeWasmModule(buff);

  var mem_1 = new ArrayBuffer(4);
  var view_1 = new Int32Array(mem_1);

  view_1[0] = 42;

  var outval_1;
  var i1 = new WebAssembly.Instance(module, {some_value: () => 1,
                                    writer: (x)=>outval_1 = x }, mem_1);

  assertEquals(43, i1.exports.main(0));

  assertEquals(42, outval_1);
})();

(function DeserializeInvalidObject() {
  var invalid_buffer = new ArrayBuffer(10);

  module = %DeserializeWasmModule(invalid_buffer);
  assertEquals(module, undefined);
})();

(function RelationBetweenModuleAndClone() {
  let builder = new WasmModuleBuilder();
  builder.addFunction("main", kSig_i)
    .addBody([kExprI8Const, 42])
    .exportFunc();

  var compiled_module = new WebAssembly.Module(builder.toBuffer());
  var serialized = %SerializeWasmModule(compiled_module);
  var clone = %DeserializeWasmModule(serialized);

  assertNotNull(clone);
  assertFalse(clone == undefined);
  assertFalse(clone == compiled_module);
  assertEquals(clone.constructor, compiled_module.constructor);
})();

(function SerializeAfterInstantiation() {
  let builder = new WasmModuleBuilder();
  builder.addFunction("main", kSig_i)
    .addBody([kExprI8Const, 42])
    .exportFunc();

  var compiled_module = new WebAssembly.Module(builder.toBuffer());
  var instance1 = new WebAssembly.Instance(compiled_module);
  var instance2 = new WebAssembly.Instance(compiled_module);
  var serialized = %SerializeWasmModule(compiled_module);
  var clone = %DeserializeWasmModule(serialized);

  assertNotNull(clone);
  assertFalse(clone == undefined);
  assertFalse(clone == compiled_module);
  assertEquals(clone.constructor, compiled_module.constructor);
  var instance3 = new WebAssembly.Instance(clone);
  assertFalse(instance3 == undefined);
})();
