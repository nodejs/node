// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags:  --turbolev --turbolev-inline-js-wasm-wrappers --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

var obj = Object.create(null);

function __f_3(wasm_fn) {
  try {
    let v = wasm_fn(obj);
    var __v_15 = [ v ];
  } catch (e) {
  }
}

function __f_4() {
    var builder = new WasmModuleBuilder();
    builder.addFunction("select", makeSig([kWasmI32], [kWasmI32]))
        .addBody([kExprUnreachable])
        .exportFunc();
    var select_fn = builder.instantiate().exports.select;

    %PrepareFunctionForOptimization(__f_3);
    __f_3(select_fn);

    %OptimizeFunctionOnNextCall(__f_3);
    __f_3(select_fn);
}
__f_4();
