// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

var builder = new WasmModuleBuilder();
builder
    .addFunction('mul', kSig_i_ii)
    // input is 2 args of type int and output is int
    .addBody([
      kExprLocalGet, 0,  // local.get i0
      kExprLocalGet, 1,  // local.get i1
      kExprI32Mul
    ])  // i32.mul i0 i1
    .exportFunc();

const instance = builder.instantiate();
const wasm_f = instance.exports.mul;

function f() {
  var result = wasm_f(21, 2);
  return result;
}

try {
  let val = f();
  f();
} catch (e) {
  print('*exception:* ' + e);
}
