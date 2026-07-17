// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Note that --no-liftoff is needed here to reproduce the issue! Without it it
// doesn't reproduce with `--variants=stress` in our test runs as that disables
// lazy compilation.
// Flags: --expose-fast-api --wasm-fast-api --no-liftoff

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let $sig0 = builder.addType(makeSig([kWasmExternRef, kWasmF32, kWasmI32], [kWasmI32]));
let foo = builder.addImport('mod', 'foo', $sig0);
builder.addFunction("main", $sig0).exportFunc().addBody([
  kExprLocalGet, 0,
  kExprLocalGet, 1,
  kExprLocalGet, 2,
  kExprCallFunction, foo,
]);

const fast_api = new d8.test.FastCAPI();
const bound = Function.prototype.call.bind(fast_api.compare_pointers);
const instance = builder.instantiate({ 'mod': { 'foo': bound } });
assertThrows(() => instance.exports.main(), TypeError, /Illegal invocation/);
