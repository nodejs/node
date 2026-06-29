// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbo-fast-api-calls --expose-fast-api --no-liftoff --wasm-fast-api
// Flags: --wasm-lazy-compilation
// Flags: --fast-api-allow-float-in-sim

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const fast_api = new d8.test.FastCAPI();

const bound_clamp_compare_i64 = Function.prototype.call.bind(fast_api.clamp_compare_i64);
const builder = new WasmModuleBuilder();
const sig_index = builder.addType(makeSig([kWasmExternRef, kWasmI32, kWasmF64, kWasmF64], [kWasmF64]));
const import_index = builder.addImport('mod', 'foo', sig_index);
builder.addFunction('main', sig_index)
    .addBody([ kExprLocalGet, 0, // receiver
               kExprLocalGet, 1, // in_range (bool as i32)
               kExprLocalGet, 2, // real_arg (f64)
               kExprLocalGet, 3, // checked_arg (f64, converted to i64 by wrapper)
               kExprCallFunction, import_index ])
    .exportFunc();

const instance = builder.instantiate({ 'mod': { 'foo': bound_clamp_compare_i64 } });

const arg1 = 1;
const arg2 = 2**63;
const arg3 = 2**63;

instance.exports.main(fast_api, arg1, arg2, arg3);
