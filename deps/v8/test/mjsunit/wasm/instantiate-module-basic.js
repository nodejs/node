// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

let kReturnValue = 117;

let buffer = (() => {
  let builder = new WasmModuleBuilder();
  builder.addMemory(1, 1, true);
  builder.addFunction("main", kSig_i)
    .addBody([kExprI8Const, kReturnValue])
    .exportFunc();

  return builder.toBuffer();
})()

function CheckInstance(instance) {
  assertFalse(instance === undefined);
  assertFalse(instance === null);
  assertFalse(instance === 0);
  assertEquals("object", typeof instance);

  // Check the memory is an ArrayBuffer.
  var mem = instance.exports.memory;
  assertFalse(mem === undefined);
  assertFalse(mem === null);
  assertFalse(mem === 0);
  assertEquals("object", typeof mem);
  assertTrue(mem instanceof WebAssembly.Memory);
  var buf = mem.buffer;
  assertTrue(buf instanceof ArrayBuffer);
  assertEquals(65536, buf.byteLength);
  for (var i = 0; i < 4; i++) {
    instance.exports.memory = 0;  // should be ignored
    mem.buffer = 0; // should be ignored
    assertSame(mem, instance.exports.memory);
    assertSame(buf, mem.buffer);
  }

  // Check the properties of the main function.
  let main = instance.exports.main;
  assertFalse(main === undefined);
  assertFalse(main === null);
  assertFalse(main === 0);
  assertEquals("function", typeof main);

  assertEquals(kReturnValue, main());
}

// Deprecated experimental API.
CheckInstance(Wasm.instantiateModule(buffer));

// Official API
let module = new WebAssembly.Module(buffer);
CheckInstance(new WebAssembly.Instance(module));

let promise = WebAssembly.compile(buffer);
promise.then(module => CheckInstance(new WebAssembly.Instance(module)));

// Check that validate works correctly for a module.
assertTrue(WebAssembly.validate(buffer));
assertFalse(WebAssembly.validate(bytes(88, 88, 88, 88, 88, 88, 88, 88)));

// Negative tests.
(function InvalidModules() {
  print("InvalidModules...");
  let invalid_cases = [undefined, 1, "", "a", {some:1, obj: "b"}];
  let len = invalid_cases.length;
  for (var i = 0; i < len; ++i) {
    try {
      let instance = new WebAssembly.Instance(invalid_cases[i]);
      assertUnreachable("should not be able to instantiate invalid modules.");
    } catch (e) {
      assertContains("Argument 0", e.toString());
    }
  }
})();

// Compile async an invalid blob.
(function InvalidBinaryAsyncCompilation() {
  print("InvalidBinaryAsyncCompilation...");
  let builder = new WasmModuleBuilder();
  builder.addFunction("f", kSig_i_i)
    .addBody([kExprCallFunction, 0]);
  let promise = WebAssembly.compile(builder.toBuffer());
  promise
    .then(compiled =>
       assertUnreachable("should not be able to compile invalid blob."))
    .catch(e => assertContains("invalid signature index", e.toString()));
})();

// Multiple instances tests.
(function ManyInstances() {
  print("ManyInstances...");
  let compiled_module = new WebAssembly.Module(buffer);
  let instance_1 = new WebAssembly.Instance(compiled_module);
  let instance_2 = new WebAssembly.Instance(compiled_module);
  assertTrue(instance_1 != instance_2);
})();

(function ManyInstancesAsync() {
  print("ManyInstancesAsync...");
  let promise = WebAssembly.compile(buffer);
  promise.then(compiled_module => {
    let instance_1 = new WebAssembly.Instance(compiled_module);
    let instance_2 = new WebAssembly.Instance(compiled_module);
    assertTrue(instance_1 != instance_2);
  });
})();

(function InstancesAreIsolatedFromEachother() {
  print("InstancesAreIsolatedFromEachother...");
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
  var mem_1 = new ArrayBuffer(4);
  var mem_2 = new ArrayBuffer(4);
  var view_1 = new Int32Array(mem_1);
  var view_2 = new Int32Array(mem_2);

  view_1[0] = 42;
  view_2[0] = 1000;

  var outval_1;
  var outval_2;
  var i1 = new WebAssembly.Instance(module, {some_value: () => 1,
                                    writer: (x)=>outval_1 = x }, mem_1);
  var i2 = new WebAssembly.Instance(module, {some_value: () => 2,
                                    writer: (x)=>outval_2 = x }, mem_2);

  assertEquals(43, i1.exports.main(0));
  assertEquals(1002, i2.exports.main(0));

  assertEquals(42, outval_1);
  assertEquals(1000, outval_2);
})();

(function GlobalsArePrivateToTheInstance() {
  print("GlobalsArePrivateToTheInstance...");
    var builder = new WasmModuleBuilder();
    builder.addGlobal(kAstI32);
    builder.addFunction("read", kSig_i_v)
        .addBody([
            kExprGetGlobal, 0])
        .exportFunc();

    builder.addFunction("write", kSig_v_i)
        .addBody([
            kExprGetLocal, 0,
            kExprSetGlobal, 0])
        .exportFunc();

    var module = new WebAssembly.Module(builder.toBuffer());
    var i1 = new WebAssembly.Instance(module);
    var i2 = new WebAssembly.Instance(module);
    i1.exports.write(1);
    i2.exports.write(2);
    assertEquals(1, i1.exports.read());
    assertEquals(2, i2.exports.read());
})();


(function InstanceMemoryIsIsolated() {
  print("InstanceMemoryIsIsolated...");
  var builder = new WasmModuleBuilder();
  builder.addMemory(1,1, true);

  builder.addFunction("f", kSig_i)
    .addBody([
      kExprI32Const, 0,
      kExprI32LoadMem, 0, 0
    ]).exportFunc();

  var mem_1 = new ArrayBuffer(65536);
  var mem_2 = new ArrayBuffer(65536);
  var view_1 = new Int32Array(mem_1);
  var view_2 = new Int32Array(mem_2);
  view_1[0] = 1;
  view_2[0] = 1000;

  var module = new WebAssembly.Module(builder.toBuffer());
  var i1 = new WebAssembly.Instance(module, null, mem_1);
  var i2 = new WebAssembly.Instance(module, null, mem_2);

  assertEquals(1, i1.exports.f());
  assertEquals(1000, i2.exports.f());
})();
