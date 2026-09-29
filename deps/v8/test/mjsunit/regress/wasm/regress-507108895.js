// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbo-fast-api-calls --expose-fast-api --wasm-fast-api --no-liftoff

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
const fast_api = new d8.test.FastCAPI();
const bound = Function.prototype.call.bind(fast_api.add_all);
const builder = new WasmModuleBuilder();
const sig_index = builder.addType(makeSig(
    [
      kWasmExternRef, kWasmI32, kWasmI32, kWasmF64, kWasmF64, kWasmF32, kWasmF64
    ],
    [kWasmF64]));
const import_index = builder.addImport('mod', 'foo', sig_index);
builder.addFunction('main', sig_index)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1, kExprLocalGet, 2, kExprLocalGet, 3,
      kExprLocalGet, 4, kExprLocalGet, 5, kExprLocalGet, 6, kExprCallFunction,
      import_index
    ])
    .exportFunc();

const instance = builder.instantiate({'mod': {'foo': bound}});
fast_api.reset_counts();
// Pass NaN as the int64 parameter. This prevents the fast call from happening.
instance.exports.main(fast_api, 1, 2, NaN, 4.0, 5.0, 6.0);
// We don't check the result, as it is unclear what it should be because of the
// NaN. However, we expect that at least a call is happening.
assertEquals(1, fast_api.fast_call_count() + fast_api.slow_call_count());
