// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO (mtrofin): re-enable ignition (v8:5345)
// Flags: --no-ignition --no-ignition-staging
// Flags: --expose-wasm --expose-gc --allow-natives-syntax

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");


(function CompiledModuleInstancesAreGCed() {
  var builder = new WasmModuleBuilder();

  builder.addMemory(1,1, true);
  builder.addImport("getValue", kSig_i);
  builder.addFunction("f", kSig_i)
    .addBody([
      kExprCallFunction, 0
    ]).exportFunc();

  var module = new WebAssembly.Module(builder.toBuffer());
  %ValidateWasmModuleState(module);
  %ValidateWasmInstancesChain(module, 0);
  var i1 = new WebAssembly.Instance(module, {getValue: () => 1});
  %ValidateWasmInstancesChain(module, 1);
  var i2 = new WebAssembly.Instance(module, {getValue: () => 2});
  %ValidateWasmInstancesChain(module, 2);
  var i3 = new WebAssembly.Instance(module, {getValue: () => 3});
  %ValidateWasmInstancesChain(module, 3);

  assertEquals(1, i1.exports.f());
  i1 = null;
  gc();
  %ValidateWasmInstancesChain(module, 2);
  assertEquals(3, i3.exports.f());
  i3 = null;
  gc();
  %ValidateWasmInstancesChain(module, 1);
  assertEquals(2, i2.exports.f());
  i2 = null;
  gc();
  %ValidateWasmModuleState(module);
  var i4 = new WebAssembly.Instance(module, {getValue: () => 4});
  assertEquals(4, i4.exports.f());
  module = null;
  gc();
  %ValidateWasmOrphanedInstance(i4);
})();
