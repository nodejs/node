// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

var builder = new WasmModuleBuilder();
builder
    .addFunction('mul', kSig_f_ff)
    // input is 2 args of type float and output is float
    .addBody([
      kExprLocalGet, 0,  // local.get f0
      kExprLocalGet, 1,  // local.get f1
      kExprF32Mul,       // f32.mul i0 i1
    ])
    .exportFunc();

const instance = builder.instantiate();
const wasm_f = instance.exports.mul;

function f() {
  var result = wasm_f(12.0, 3.5);
  return result;
}

try {
  let val = f();
  print('float result: ' + val);
} catch (e) {
  print('*exception:* ' + e);
}
