// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-test-streaming

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function TestAsyncCompileTagSection() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addTag(kSig_v_v);
  builder.addFunction("thrw", kSig_v_v)
      .addBody([
        kExprThrow, except,
      ]).exportFunc();
  function step1(buffer) {
    assertPromiseResult(WebAssembly.compile(buffer), module => step2(module));
  }
  function step2(module) {
    assertPromiseResult(WebAssembly.instantiate(module), inst => step3(inst));
  }
  function step3(instance) {
    assertThrows(() => instance.exports.thrw(), WebAssembly.Exception);
  }
  step1(builder.toBuffer());
})();
