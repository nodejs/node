// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

const kReturnValue = 15;

function getBuilder() {
  var builder = new WasmModuleBuilder();

  builder.addFunction("main", kSig_i_i)
    .addBody([kExprI32Const, kReturnValue])
    .exportFunc();
  return builder;
}

(function BasicTest() {
  var builder = getBuilder();
  var main = builder.instantiate().exports.main;
  assertEquals(kReturnValue, main());
})();

(function AsyncTest() {
  var builder = getBuilder();
  var buffer = builder.toBuffer();
  assertPromiseResult(
    WebAssembly.instantiate(buffer)
      .then(pair => pair.instance.exports.main(), assertUnreachable)
      .then(result => assertEquals(kReturnValue, result), assertUnreachable));
})();
