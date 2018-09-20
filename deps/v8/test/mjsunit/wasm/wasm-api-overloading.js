// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

let buffer = (() => {
  let builder = new WasmModuleBuilder();
  builder.addFunction("f", kSig_i_v)
    .addBody([kExprI32Const, 42])
    .exportAs("f");
  return builder.toBuffer();
})();

var module = new WebAssembly.Module(buffer);
var wrapper = [module];

try {
  assertPromiseResult(
    WebAssembly.instantiateStreaming(wrapper),
    assertUnreachable, assertUnreachable);
} catch (e) {
  assertTrue(e instanceof TypeError);
}

try {
  assertPromiseResult(
    WebAssembly.compileStreaming(wrapper),
    assertUnreachable, assertUnreachable);
} catch (e) {
  assertTrue(e instanceof TypeError);
}
