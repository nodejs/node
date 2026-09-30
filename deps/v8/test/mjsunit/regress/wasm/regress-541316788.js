// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-fast-api --expose-fast-api --no-liftoff

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const fast_c_api = new d8.test.FastCAPI();
const builder = new WasmModuleBuilder();
builder.addMemory();
const call_to_number = builder.addImport(
    'env', 'call_to_number', makeSig([kWasmExternRef, kWasmExternRef], []));
builder.addFunction('test', makeSig([kWasmExternRef, kWasmExternRef], []))
    .exportFunc()
    .addBody([
      kExprLocalGet,
      0,
      kExprLocalGet,
      1,
      kExprCallFunction,
      call_to_number,
    ]);

const instance = builder.instantiate({
  env: {
    call_to_number: Function.call.bind(fast_c_api.call_to_number),
  },
});

const arg = {
  valueOf: () => {
    console.trace();
  }
};

instance.exports.test(fast_c_api, arg);
