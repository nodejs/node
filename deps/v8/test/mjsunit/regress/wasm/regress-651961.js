// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

(function() {
  var builder = new WasmModuleBuilder();
  builder.addMemory(1, 32, false);
  builder.addFunction("foo", kSig_i_v)
    .addBody([
              kExprMemorySize, kMemoryZero,
              kExprI32Const, 0x10,
              kExprGrowMemory, kMemoryZero,
              kExprI32Mul,
              ])
              .exportFunc();
  var module = builder.instantiate();
  var result = module.exports.foo();
  assertEquals(1, result);
})();
