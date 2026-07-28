// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --expose-fast-api --wasm-fast-api --no-liftoff

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let $sig_enforce =
    builder.addType(makeSig([kWasmExternRef, kWasmF64, kWasmF64], [kWasmI32]));
let enforce = builder.addImport('mod', 'enforce', $sig_enforce);
builder.addFunction('main', $sig_enforce).exportFunc().addBody([
  kExprLocalGet, 0,   // .
  kExprLocalGet, 1,   // .
  kExprLocalGet, 2,   // .
  kExprCallFunction,  // .
  enforce,            // .
]);

const fast_api = new d8.test.FastCAPI();
const bound = Function.prototype.call.bind(fast_api.enforce_range_compare_u64);
const instance = builder.instantiate({'mod': {'enforce': bound}});
try {
  let res = instance.exports.main(fast_api, 10.0, -1.0);
  print('Result: ' + res);
} catch (e) {
  print('Caught: ' + e);
}
