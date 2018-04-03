// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-threads

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

function assertMemoryIsValid(memory, shared) {
  assertSame(WebAssembly.Memory.prototype, memory.__proto__);
  assertSame(WebAssembly.Memory, memory.constructor);
  assertTrue(memory instanceof Object);
  assertTrue(memory instanceof WebAssembly.Memory);
  if (shared) {
    assertTrue(memory.buffer instanceof SharedArrayBuffer);
    // Assert that the buffer is frozen when memory is shared.
    assertTrue(Object.isFrozen(memory.buffer));
  }
}

(function TestConstructorWithShared() {
  print("TestConstructorWithShared");
  let memory = new WebAssembly.Memory({
    initial: 0, maximum: 10, shared: true});
  assertMemoryIsValid(memory, true);
})();

(function TestConstructorWithUndefinedShared() {
  print("TestConstructorWithUndefinedShared");
  // Maximum = undefined, shared = undefined.
  let memory = new WebAssembly.Memory({
    initial: 0, maximum: undefined, shared: undefined});
  assertMemoryIsValid(memory);
})();

(function TestConstructorWithNumericShared() {
  print("TestConstructorWithNumericShared");
  // For numeric values, shared = true.
  let memory = new WebAssembly.Memory({
    initial: 0, maximum: 10, shared: 2098665});
  assertMemoryIsValid(memory, true);
})();

(function TestConstructorWithEmptyStringShared() {
  print("TestConstructorWithEmptyStringShared");
  // Maximum = undefined, shared = false.
  let memory = new WebAssembly.Memory({
    initial: 0, maximum: undefined, shared: ""});
  assertMemoryIsValid(memory);
})();

(function TestConstructorWithUndefinedMaxShared() {
  print("TestConstructorWithUndefinedMaxShared");
  // New memory with Maximum = undefined, shared = true => TypeError.
  assertThrows(() => new WebAssembly.Memory({initial: 0, shared: true}),
      TypeError);
})();

(function TestCompileWithUndefinedShared() {
  print("TestCompileWithUndefinedShared");
  let memory = new WebAssembly.Memory({
    initial: 0, maximum: 10, shared: true});
  let builder = new WasmModuleBuilder();
  builder.addImportedMemory("m", "imported_mem", 0, undefined, "shared");
  assertThrows(() => new WebAssembly.Module(builder.toBuffer()),
       WebAssembly.CompileError);
})();

(function TestCompileAtomicOpUndefinedShared() {
  print("TestCompileAtomicOpUndefinedShared");
  let memory = new WebAssembly.Memory({
    initial: 0, maximum: 10, shared: true});
  let builder = new WasmModuleBuilder();
  builder.addFunction("main", kSig_i_ii)
    .addBody([
      kExprGetLocal, 0,
      kExprGetLocal, 1,
      kAtomicPrefix,
      kExprI32AtomicAdd]);
  builder.addImportedMemory("m", "imported_mem");
  assertThrows(() => new WebAssembly.Module(builder.toBuffer()),
       WebAssembly.CompileError);
})();

(function TestInstantiateWithUndefinedShared() {
  print("TestInstantiateWithUndefinedShared");
  let memory = new WebAssembly.Memory({
    initial: 0, maximum: 10, shared: true});
  let builder = new WasmModuleBuilder();
  builder.addImportedMemory("m", "imported_mem");
  let module = new WebAssembly.Module(builder.toBuffer());
  assertThrows(() => new WebAssembly.Instance(module,
         {m: {imported_mem: memory}}), WebAssembly.LinkError);
})();

(function TestInstantiateWithImportNotSharedDefined() {
  print("TestInstantiateWithImportNotSharedDefined");
  let memory = new WebAssembly.Memory({
    initial: 0, maximum: 10, shared: false});
  let builder = new WasmModuleBuilder();
  builder.addImportedMemory("m", "imported_mem", 0, 10, "shared");
  let module = new WebAssembly.Module(builder.toBuffer());
  assertThrows(() => new WebAssembly.Instance(module,
         {m: {imported_mem: memory}}), WebAssembly.LinkError);
})();

(function TestInstantiateWithSharedDefined() {
  print("TestInstantiateWithSharedDefined");
  let builder = new WasmModuleBuilder();
  builder.addMemory(2, 10, true, "shared");
  let module = new WebAssembly.Module(builder.toBuffer());
  let instance = new WebAssembly.Instance(module);
  assertMemoryIsValid(instance.exports.memory, true);
})();

(function TestAtomicOpWithSharedMemoryDefined() {
  print("TestAtomicOpWithSharedMemoryDefined");
  let builder = new WasmModuleBuilder();
  builder.addMemory(2, 10, false, "shared");
  builder.addFunction("main", kSig_i_ii)
    .addBody([
      kExprGetLocal, 0,
      kExprGetLocal, 1,
      kAtomicPrefix,
      kExprI32AtomicAdd, 2, 0])
    .exportFunc();
  let module = new WebAssembly.Module(builder.toBuffer());
  let instance = new WebAssembly.Instance(module);
  assertEquals(0, instance.exports.main(0, 0x11111111));
  assertEquals(0x11111111, instance.exports.main(0, 0x11111111));
})();
