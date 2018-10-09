// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --trace-turbo --trace-turbo-graph
// Flags: --trace-turbo-cfg-file=test/mjsunit/tools/turbo.cfg
// Flags: --trace-turbo-path=test/mjsunit/tools

load('test/mjsunit/wasm/wasm-constants.js');
load('test/mjsunit/wasm/wasm-module-builder.js');

// The idea behind this test is to make sure we do not crash when using the
// --trace-turbo flag given different sort of inputs, JS or WASM.

(function testOptimizedJS() {
  function add(a, b) {
    return a + b;
  }

  add(21, 21);
  %OptimizeFunctionOnNextCall(add);
  add(20, 22);
})();

(function testWASM() {
  let builder = new WasmModuleBuilder();

  builder.addFunction("add", kSig_i_ii)
    .addBody([kExprGetLocal, 0,
              kExprGetLocal, 1,
              kExprI32Add])
    .exportFunc();

  let instance = builder.instantiate();
  instance.exports.add(21, 21);
})();
