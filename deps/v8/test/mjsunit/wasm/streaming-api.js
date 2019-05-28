// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-test-streaming

load("test/mjsunit/wasm/wasm-module-builder.js");

(function TestCompileStreaming() {
    print(arguments.callee.name);
    let builder = new WasmModuleBuilder();
    builder.addFunction("main", kSig_i_i)
    .addBody([kExprGetLocal, 0])
    .exportAs("main");
  let bytes = builder.toBuffer();
  assertPromiseResult(WebAssembly.compileStreaming(Promise.resolve(bytes)).then(
    module => WebAssembly.instantiate(module)).then(
      instance => assertEquals(5, instance.exports.main(5))));
})();

(function TestInstantiateStreaming() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction("main", kSig_i_i)
  .addBody([kExprGetLocal, 0])
  .exportAs("main");
let bytes = builder.toBuffer();
assertPromiseResult(WebAssembly.instantiateStreaming(Promise.resolve(bytes)).then(
  ({module, instance}) => assertEquals(5, instance.exports.main(5))));
})();

(function TestCompileStreamingRejectedInputPromise() {
  print(arguments.callee.name);
  assertPromiseResult(WebAssembly.compileStreaming(Promise.reject("myError")),
    assertUnreachable,
    error => assertEquals(error, "myError"));
})();

(function TestInstantiateStreamingRejectedInputPromise() {
  print(arguments.callee.name);
  assertPromiseResult(WebAssembly.instantiateStreaming(Promise.reject("myError")),
    assertUnreachable,
    error => assertEquals(error, "myError"));
})();
