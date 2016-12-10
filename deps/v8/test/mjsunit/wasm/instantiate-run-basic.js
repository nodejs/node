// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

(function BasicTest() {
  var kReturnValue = 107;
  var builder = new WasmModuleBuilder();

  builder.addFunction("main", kSig_i_i)
    .addBody([kExprI8Const, kReturnValue])
    .exportFunc();

  var main = builder.instantiate().exports.main;
  assertEquals(kReturnValue, main());
})();
