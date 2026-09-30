// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --turbo-inline-js-wasm-calls

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let array_type = builder.addArray(kWasmI32);
let struct_type = builder.addStruct([makeField(kWasmI32, true)]);

let make_array = builder.addFunction('make_array', makeSig([], [kWasmExternRef])).exportFunc();
make_array.addBody([
  kExprI32Const, 10,
  kGCPrefix, kExprArrayNewDefault, array_type,
  kGCPrefix, kExprExternConvertAny,
]);

let make_struct = builder.addFunction('make_struct', makeSig([], [kWasmExternRef])).exportFunc();
make_struct.addBody([
  kExprI32Const, 42,
  kGCPrefix, kExprStructNew, struct_type,
  kGCPrefix, kExprExternConvertAny,
]);

// 1. Abstract cast: ref.cast array
let abstract_cast = builder.addFunction('abstract_cast', makeSig([kWasmExternRef], [kWasmExternRef])).exportFunc();
abstract_cast.addBody([
  kExprLocalGet, 0,
  kGCPrefix, kExprAnyConvertExtern,
  kGCPrefix, kExprRefCast, kArrayRefCode,
  kGCPrefix, kExprExternConvertAny,
]);

// 2. Abstract cast: ref.cast null array
let abstract_cast_null = builder.addFunction('abstract_cast_null', makeSig([kWasmExternRef], [kWasmExternRef])).exportFunc();
abstract_cast_null.addBody([
  kExprLocalGet, 0,
  kGCPrefix, kExprAnyConvertExtern,
  kGCPrefix, kExprRefCastNull, kArrayRefCode,
  kGCPrefix, kExprExternConvertAny,
]);

// 3. Concrete cast: ref.cast <array_type>
let concrete_cast = builder.addFunction('concrete_cast', makeSig([kWasmExternRef], [kWasmExternRef])).exportFunc();
concrete_cast.addBody([
  kExprLocalGet, 0,
  kGCPrefix, kExprAnyConvertExtern,
  kGCPrefix, kExprRefCast, array_type,
  kGCPrefix, kExprExternConvertAny,
]);

// 4. Concrete cast: ref.cast null <array_type>
let concrete_cast_null = builder.addFunction('concrete_cast_null', makeSig([kWasmExternRef], [kWasmExternRef])).exportFunc();
concrete_cast_null.addBody([
  kExprLocalGet, 0,
  kGCPrefix, kExprAnyConvertExtern,
  kGCPrefix, kExprRefCastNull, array_type,
  kGCPrefix, kExprExternConvertAny,
]);

// 5. Abstract cast then array.len
let cast_then_len = builder.addFunction('cast_then_len', makeSig([kWasmExternRef], [kWasmI32])).exportFunc();
cast_then_len.addBody([
  kExprLocalGet, 0,
  kGCPrefix, kExprAnyConvertExtern,
  kGCPrefix, kExprRefCast, kArrayRefCode,
  kGCPrefix, kExprArrayLen,
]);

let cast_null_then_len = builder.addFunction('cast_null_then_len', makeSig([kWasmExternRef], [kWasmI32])).exportFunc();
cast_null_then_len.addBody([
  kExprLocalGet, 0,
  kGCPrefix, kExprAnyConvertExtern,
  kGCPrefix, kExprRefCastNull, kArrayRefCode,
  kGCPrefix, kExprArrayLen,
]);

// 6. Direct array.len and struct.get with nullable parameter
let array_len = builder.addFunction('array_len', makeSig([wasmRefNullType(array_type)], [kWasmI32])).exportFunc();
array_len.addBody([
  kExprLocalGet, 0,
  kGCPrefix, kExprArrayLen,
]);

let struct_get = builder.addFunction('struct_get', makeSig([wasmRefNullType(struct_type)], [kWasmI32])).exportFunc();
struct_get.addBody([
  kExprLocalGet, 0,
  kGCPrefix, kExprStructGet, struct_type, 0,
]);

const instance = builder.instantiate();
const wasm_array = instance.exports.make_array();
const wasm_struct = instance.exports.make_struct();

function testOptimized(run, fctToOptimize) {
  %DeoptimizeFunction(fctToOptimize);
  %PrepareFunctionForOptimization(fctToOptimize);
  run();
  %OptimizeFunctionOnNextCall(fctToOptimize);
  run();
  assertOptimized(fctToOptimize);
}

// Test abstract cast (non-nullable)
function test_abstract(x) {
  return instance.exports.abstract_cast(x);
}
testOptimized(() => {
  assertSame(wasm_array, test_abstract(wasm_array));
  assertThrows(() => test_abstract(null), WebAssembly.RuntimeError, /illegal cast/);
  assertThrows(() => test_abstract(undefined), WebAssembly.RuntimeError, /illegal cast/);
  assertThrows(() => test_abstract({}), WebAssembly.RuntimeError, /illegal cast/);
  assertThrows(() => test_abstract(123), WebAssembly.RuntimeError, /illegal cast/);
}, test_abstract);

// Test abstract cast (nullable)
function test_abstract_null(x) {
  return instance.exports.abstract_cast_null(x);
}
testOptimized(() => {
  assertSame(wasm_array, test_abstract_null(wasm_array));
  assertEquals(null, test_abstract_null(null));
  assertThrows(() => test_abstract_null(undefined), WebAssembly.RuntimeError, /illegal cast/);
  assertThrows(() => test_abstract_null({}), WebAssembly.RuntimeError, /illegal cast/);
  assertThrows(() => test_abstract_null(123), WebAssembly.RuntimeError, /illegal cast/);
}, test_abstract_null);

// Test concrete cast (non-nullable)
function test_concrete(x) {
  return instance.exports.concrete_cast(x);
}
testOptimized(() => {
  assertSame(wasm_array, test_concrete(wasm_array));
  assertThrows(() => test_concrete(null), WebAssembly.RuntimeError, /illegal cast/);
  assertThrows(() => test_concrete(wasm_struct), WebAssembly.RuntimeError, /illegal cast/);
}, test_concrete);

// Test concrete cast (nullable)
function test_concrete_null(x) {
  return instance.exports.concrete_cast_null(x);
}
testOptimized(() => {
  assertSame(wasm_array, test_concrete_null(wasm_array));
  assertEquals(null, test_concrete_null(null));
  assertThrows(() => test_concrete_null(wasm_struct), WebAssembly.RuntimeError, /illegal cast/);
}, test_concrete_null);

// Test cast then array.len
function test_cast_len(x) {
  return instance.exports.cast_then_len(x);
}
testOptimized(() => {
  assertEquals(10, test_cast_len(wasm_array));
  assertThrows(() => test_cast_len(null), WebAssembly.RuntimeError, /illegal cast/);
}, test_cast_len);

function test_cast_null_len(x) {
  return instance.exports.cast_null_then_len(x);
}
testOptimized(() => {
  assertEquals(10, test_cast_null_len(wasm_array));
  assertThrows(() => test_cast_null_len(null), WebAssembly.RuntimeError, /null pointer/);
}, test_cast_null_len);

// Test direct array.len on null
function test_len(x) {
  return instance.exports.array_len(x);
}
testOptimized(() => {
  assertEquals(10, test_len(wasm_array));
  assertThrows(() => test_len(null), WebAssembly.RuntimeError, /null pointer/);
}, test_len);

// Test direct struct.get on null
function test_get(x) {
  return instance.exports.struct_get(x);
}
testOptimized(() => {
  assertEquals(42, test_get(wasm_struct));
  assertThrows(() => test_get(null), WebAssembly.RuntimeError, /null pointer/);
}, test_get);
