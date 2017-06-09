// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --expose-gc --allow-natives-syntax

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

// Use global variables for all values where the test wants to maintain strict
// control over value lifetime. Using local variables would not give sufficient
// guarantees of the value lifetime.
var module;
var instance1;
var instance2;
var instance3;
var instance4;

(function CompiledModuleInstancesInitialize1to3() {
  var builder = new WasmModuleBuilder();

  builder.addMemory(1,1, true);
  builder.addImport("", "getValue", kSig_i_v);
  builder.addFunction("f", kSig_i_v)
    .addBody([
      kExprCallFunction, 0
    ]).exportFunc();

  module = new WebAssembly.Module(builder.toBuffer());
  %ValidateWasmModuleState(module);
  %ValidateWasmInstancesChain(module, 0);
  instance1 = new WebAssembly.Instance(module, {"": {getValue: () => 1}});
  %ValidateWasmInstancesChain(module, 1);
  instance2 = new WebAssembly.Instance(module, {"": {getValue: () => 2}});
  %ValidateWasmInstancesChain(module, 2);
  instance3 = new WebAssembly.Instance(module, {"": {getValue: () => 3}});
  %ValidateWasmInstancesChain(module, 3);
})();

(function CompiledModuleInstancesClear1() {
  assertEquals(1, instance1.exports.f());
  instance1 = null;
})();

gc();
%ValidateWasmInstancesChain(module, 2);

(function CompiledModuleInstancesClear3() {
  assertEquals(3, instance3.exports.f());
  instance3 = null;
})();

gc();
%ValidateWasmInstancesChain(module, 1);

(function CompiledModuleInstancesClear2() {
  assertEquals(2, instance2.exports.f());
  instance2 = null;
})();

gc();
%ValidateWasmModuleState(module);

(function CompiledModuleInstancesInitialize4AndClearModule() {
  instance4 = new WebAssembly.Instance(module, {"": {getValue: () => 4}});
  assertEquals(4, instance4.exports.f());
  module = null;
})();

gc();
%ValidateWasmOrphanedInstance(instance4);
