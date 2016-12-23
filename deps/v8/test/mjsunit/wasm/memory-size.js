// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

(function testMemorySizeZero() {
  print("testMemorySizeZero()");
  var builder = new WasmModuleBuilder();
  builder.addFunction("memory_size", kSig_i_v)
         .addBody([kExprMemorySize])
         .exportFunc();
  var module = builder.instantiate();
  assertEquals(0, module.exports.memory_size());
})();

(function testMemorySizeNonZero() {
  print("testMemorySizeNonZero()");
  var builder = new WasmModuleBuilder();
  var size = 11;
  builder.addMemory(size, size, false);
  builder.addFunction("memory_size", kSig_i_v)
         .addBody([kExprMemorySize])
         .exportFunc();
  var module = builder.instantiate();
  assertEquals(size, module.exports.memory_size());
})();
