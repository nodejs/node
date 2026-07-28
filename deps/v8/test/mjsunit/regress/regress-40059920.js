// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (typeof WebAssembly !== 'undefined') {
  d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

  function TestGenericWrapper(params_and_args) {
    let builder = new WasmModuleBuilder();

    let params = [];
    let args = [];
    let func_names = [];
    for (let pna of params_and_args) {
      params.push(pna.type);
      args.push(pna.value);
    }

    for (let i = 0; i < params_and_args.length; i++) {
      let sig = makeSig(params, [params_and_args[i].type]);
      let func_name = `func${i}`;
      func_names.push(func_name);
      builder.addFunction(func_name, sig).addBody([
        kExprLocalGet, i,
      ]).exportFunc();
    }


    let instance = builder.instantiate();
    for (let i = 0; i < params_and_args.length; i++) {
      let input = args[i].toString(16);
      let output = instance.exports[func_names[i]](...args);
      console.log(`${input} -> ${output.toString(16)}`);
      assertEquals(args[i], output);
    }
  }

  TestGenericWrapper([
    {type: kWasmI32, value: 0x1},
    {type: kWasmI32, value: 0x2},
    {type: kWasmI32, value: 0x3},
    {type: kWasmI32, value: 0x4},
    {type: kWasmI32, value: 0x5},
    {type: kWasmI32, value: 0x6},
    {type: kWasmI32, value: 0x7},
    {type: kWasmF64, value: 0x8},
    {type: kWasmF64, value: 0x9},
    {type: kWasmF64, value: 0xa},
    {type: kWasmF64, value: 0xb},
    {type: kWasmF64, value: 0xc},
    {type: kWasmF64, value: 0xd},
    {type: kWasmF64, value: 0xe},
    {type: kWasmF64, value: 0xf},
    {type: kWasmAnyRef, value: 0x11},
    {type: kWasmAnyRef, value: 0x222},
    {type: kWasmAnyRef, value: 0x3333},
    {type: kWasmAnyRef, value: 0x44444},
  ]);

  TestGenericWrapper([
        // Fill up the registers.
    {type: kWasmI32, value: 0x11},
    {type: kWasmI32, value: 0x12},
    {type: kWasmI32, value: 0x13},
    {type: kWasmI32, value: 0x14},
    {type: kWasmI32, value: 0x15},
    {type: kWasmF64, value: 0x16},
    {type: kWasmF64, value: 0x17},
    {type: kWasmF64, value: 0x18},
    {type: kWasmF64, value: 0x19},
    {type: kWasmF64, value: 0x1a},
    {type: kWasmF64, value: 0x1b},
    // Have some fun on the stack.
    {type: kWasmI32, value: 0x1},
    {type: kWasmF64, value: 0x2},
    {type: kWasmAnyRef, value: 0x3},
    {type: kWasmI32, value: 0x4},
    {type: kWasmF64, value: 0x5},
    {type: kWasmAnyRef, value: 0x6},
    {type: kWasmI32, value: 0x7},
    {type: kWasmF64, value: 0x8},
    {type: kWasmAnyRef, value: 0x9},
    {type: kWasmI32, value: 0xa},
    {type: kWasmF64, value: 0xb},
    {type: kWasmAnyRef, value: 0xc},
    {type: kWasmI32, value: 0xd},
    {type: kWasmF64, value: 0xe},
    {type: kWasmAnyRef, value: 0xf},
  ]);

  TestGenericWrapper([
        // Fill up the registers.
    {type: kWasmI32, value: 0x11},
    {type: kWasmI32, value: 0x12},
    {type: kWasmI32, value: 0x13},
    {type: kWasmI32, value: 0x14},
    {type: kWasmI32, value: 0x15},
    {type: kWasmF64, value: 0x16},
    {type: kWasmF64, value: 0x17},
    {type: kWasmF64, value: 0x18},
    {type: kWasmF64, value: 0x19},
    {type: kWasmF64, value: 0x1a},
    {type: kWasmF64, value: 0x1b},
    // Have some fun on the stack.
    {type: kWasmI64, value: 0x1n},
    {type: kWasmF32, value: 0x2},
    {type: kWasmAnyRef, value: 0x3},
    {type: kWasmI32, value: 0x4},
    {type: kWasmF64, value: 0x5},
    {type: kWasmAnyRef, value: 0x6},
    {type: kWasmI64, value: 0x7n},
    {type: kWasmF64, value: 0x8},
    {type: kWasmAnyRef, value: 0x9},
    {type: kWasmI32, value: 0xa},
    {type: kWasmF32, value: 0xb},
    {type: kWasmAnyRef, value: 0xc},
    {type: kWasmI64, value: 0xdn},
    {type: kWasmF32, value: 0xe},
    {type: kWasmAnyRef, value: 0xf},
  ]);
}
