// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-constants.js');
load('test/mjsunit/wasm/wasm-module-builder.js');

let types = [kWasmI32, kWasmF32, kWasmF64];
let type_names = ["i32", "f32", "f64"];
let type_const = [wasmI32Const, wasmF32Const, wasmF64Const];

function f(values, shift, num_const_params, ...args) {
  assertEquals(
      values.length + num_const_params, args.length, 'number of arguments');
  args.forEach((arg_val, idx) => {
    const expected =
        idx < values.length ? values[(idx + shift) % values.length] : idx;
    assertEquals(expected, arg_val, 'arg #' + idx + ', shifted by ' + shift);
  });
}

types.forEach((type, type_idx) => {
  for (let num_params = 3; num_params < 32; num_params += 4) {
    print(
        'Testing ' + num_params + ' parameters of type ' +
        type_names[type_idx] + '...');
    for (let num_const_params = 0; num_const_params <= 3; ++num_const_params) {
      for (let shift = 2; shift <= 5; shift += 3) {
        let builder = new WasmModuleBuilder();

        let params_outer = new Array(num_params).fill(type);
        sig_outer = makeSig(params_outer, []);
        let params_inner = new Array(num_params + num_const_params).fill(type);
        sig_inner = makeSig(params_inner, []);

        let body = [];
        for (let i = 0; i < num_params; ++i)
          body.push(kExprGetLocal, (i + shift) % num_params);
        for (let i = 0; i < num_const_params; ++i)
          body.push(...type_const[type_idx](num_params + i));
        body.push(kExprCallFunction, 0);

        builder.addImport('', 'f', sig_inner);
        builder.addFunction(undefined, sig_outer)
            .addBody(body)
            .exportAs('main');
        let values = new Array(num_params).fill(0).map((_, i) => 123 - 3 * i);

        instance = builder.instantiate(
            {'': {'f': f.bind(null, values, shift, num_const_params)}});
        instance.exports.main(...values);
      }
    }
  }
});
