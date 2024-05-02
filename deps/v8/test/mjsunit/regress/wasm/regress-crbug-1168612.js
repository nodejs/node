// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-inline-js-wasm-calls

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

function getMain() {
  var builder = new WasmModuleBuilder();
  builder.addFunction("main", kSig_v_v)
    .addBody([kExprUnreachable])
    .exportAs("main");
  return builder.instantiate().exports.main;
}
let foo = getMain();

function loop() {
  for (let i = 0; i < 2; i++) {
    try {
       foo();
    } catch (e) {
      if (i) {
        throw e;
      }
    }
  }
}
%PrepareFunctionForOptimization(loop);
assertThrows(loop, WebAssembly.RuntimeError, "unreachable");
%OptimizeFunctionOnNextCall(loop);
assertThrows(loop, WebAssembly.RuntimeError, "unreachable");
