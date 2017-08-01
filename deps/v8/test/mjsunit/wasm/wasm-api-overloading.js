// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

%ResetWasmOverloads();
let buffer = (() => {
  let builder = new WasmModuleBuilder();
  builder.addFunction("f", kSig_i_v)
    .addBody([kExprI32Const, 42])
    .exportAs("f");
  return builder.toBuffer();
})();

var module = new WebAssembly.Module(buffer);
var wrapper = [module];

assertPromiseResult(
  WebAssembly.instantiateStreaming(wrapper),
  assertUnreachable,
  e => assertTrue(e instanceof TypeError));

assertPromiseResult(
  WebAssembly.compileStreaming(wrapper),
  assertUnreachable,
  e => assertTrue(e instanceof TypeError));

assertPromiseResult(
  (() => {
    %SetWasmCompileFromPromiseOverload();
    return WebAssembly.compileStreaming(wrapper);
  })(),
  module => {
    assertTrue(module instanceof WebAssembly.Module);
      %ResetWasmOverloads();
  },
  assertUnreachable);

assertPromiseResult(
  (() => {
    %SetWasmCompileFromPromiseOverload();
    return WebAssembly.instantiateStreaming(wrapper);
  })(),
  pair => {
    assertTrue(pair.instance instanceof WebAssembly.Instance);
    assertTrue(pair.module instanceof WebAssembly.Module);
    %ResetWasmOverloads();
  },
  assertUnreachable);
