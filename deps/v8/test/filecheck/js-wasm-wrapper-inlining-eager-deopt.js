// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbolev --no-maglev --turbolev-inline-js-wasm-wrappers
// Flags: --allow-natives-syntax
// Flags: --trace-deopt
// Flags: --turbofan --no-stress-maglev

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/mjsunit.js');

// Test that JS-to-Wasm wrapper inlining correctly handles numeric parameter
// conversion. The eager deopt guard rejects JSReceivers (which could trigger
// user JS code via valueOf during conversion, causing a lazy deopt crash).
// Primitives (numbers, strings, undefined, etc.) pass the guard and convert
// via the normal path with no deopt at all. See crbug.com/493307329.

let builder = new WasmModuleBuilder();

builder.addFunction("wasm_i32_arg", makeSig([kWasmI32], [kWasmI32]))
    .addBody([kExprI32Const, 42])
    .exportFunc();

builder.addFunction("wasm_f32_arg", makeSig([kWasmF32], [kWasmI32]))
    .addBody([kExprI32Const, 43])
    .exportFunc();

builder.addFunction("wasm_f64_arg", makeSig([kWasmF64], [kWasmI32]))
    .addBody([kExprI32Const, 44])
    .exportFunc();

builder.addFunction("wasm_i64_arg", makeSig([kWasmI64], [kWasmI32]))
    .addBody([kExprI32Const, 46])
    .exportFunc();

builder.addFunction("wasm_i64_identity", makeSig([kWasmI64], [kWasmI64]))
    .addBody([kExprLocalGet, 0])
    .exportFunc();

builder.addFunction("wasm_externref_arg", makeSig([kWasmExternRef], [kWasmI32]))
    .addBody([kExprI32Const, 45])
    .exportFunc();

builder.addFunction("wasm_i32_arg_extret", makeSig([kWasmI32], [kWasmExternRef]))
    .addBody([kExprRefNull, kExternRefCode])
    .exportFunc();

let instance = builder.instantiate();

function optimize_and_call(fn, arg, expected) {
  %PrepareFunctionForOptimization(fn);
  fn(arg);
  fn(arg);
  %OptimizeFunctionOnNextCall(fn);
  assertEquals(expected, fn(arg));
}

// Valid args -- Smi, HeapNumber, externref. No deopt expected.
// CHECK-NOT: deopt-lazy

function test_i32_smi(arg) { return instance.exports.wasm_i32_arg(arg); }
optimize_and_call(test_i32_smi, 2, 42);

function test_i32_heap(arg) { return instance.exports.wasm_i32_arg(arg); }
optimize_and_call(test_i32_heap, 2147483647.5, 42);

function test_f32_smi(arg) { return instance.exports.wasm_f32_arg(arg); }
optimize_and_call(test_f32_smi, 3, 43);

function test_f32_heap(arg) { return instance.exports.wasm_f32_arg(arg); }
optimize_and_call(test_f32_heap, 3.14, 43);

function test_f64_smi(arg) { return instance.exports.wasm_f64_arg(arg); }
optimize_and_call(test_f64_smi, 4, 44);

function test_f64_heap(arg) { return instance.exports.wasm_f64_arg(arg); }
optimize_and_call(test_f64_heap, 2.718281828, 44);

function test_i64_bigint(arg) { return instance.exports.wasm_i64_identity(arg); }
optimize_and_call(test_i64_bigint, 42n, 42n);

// BigInt larger than i64: should silently wrap around (ToBigInt64 modular
// truncation), no deopt expected.
function test_i64_bigint_wrap(arg) {
  return instance.exports.wasm_i64_identity(arg);
}
optimize_and_call(test_i64_bigint_wrap, (1n << 64n) + 42n, 42n);
print("test_i64_bigint_wrap done");
// CHECK-NOT: deopt
// CHECK: test_i64_bigint_wrap done

function test_externref(arg) {
  return instance.exports.wasm_externref_arg(arg);
}
optimize_and_call(test_externref, { foo: "bar" }, 45);

function test_externref_obj(arg) {
  return instance.exports.wasm_externref_arg(arg);
}
optimize_and_call(test_externref_obj, { valueOf() { return 99; } }, 45);

// Invalid args -- objects with valueOf. Must trigger eager deopt, never lazy deopt.

// --- i32: object -> eager deopt "not a Number" ---
let deopt_obj_i32 = {
  valueOf() { %DeoptimizeFunction(test_i32_obj); return 1; }
};
function test_i32_obj(arg) { return instance.exports.wasm_i32_arg(arg); }
// CHECK: [bailout (kind: deopt-eager, reason: not a Number): begin. deoptimizing {{.*}} <JSFunction test_i32_obj {{.*}}>
// CHECK-NOT: deopt-lazy
optimize_and_call(test_i32_obj, deopt_obj_i32, 42);

// --- f32: object -> eager deopt "not a Number" ---
let deopt_obj_f32 = {
  valueOf() { %DeoptimizeFunction(test_f32_obj); return 1.0; }
};
function test_f32_obj(arg) { return instance.exports.wasm_f32_arg(arg); }
// CHECK: [bailout (kind: deopt-eager, reason: not a Number): begin. deoptimizing {{.*}} <JSFunction test_f32_obj {{.*}}>
// CHECK-NOT: deopt-lazy
optimize_and_call(test_f32_obj, deopt_obj_f32, 43);

// --- f64: object -> eager deopt "not a Number" ---
let deopt_obj_f64 = {
  valueOf() { %DeoptimizeFunction(test_f64_obj); return 1.0; }
};
function test_f64_obj(arg) { return instance.exports.wasm_f64_arg(arg); }
// CHECK: [bailout (kind: deopt-eager, reason: not a Number): begin. deoptimizing {{.*}} <JSFunction test_f64_obj {{.*}}>
// CHECK-NOT: deopt-lazy
optimize_and_call(test_f64_obj, deopt_obj_f64, 44);

// --- i64: object -> eager deopt "not a BigInt" ---
let deopt_obj_i64 = {
  valueOf() { %DeoptimizeFunction(test_i64_obj); return 1n; }
};
function test_i64_obj(arg) { return instance.exports.wasm_i64_arg(arg); }
// CHECK: [bailout (kind: deopt-eager, reason: not a BigInt): begin. deoptimizing {{.*}} <JSFunction test_i64_obj {{.*}}>
// CHECK-NOT: deopt-lazy
optimize_and_call(test_i64_obj, deopt_obj_i64, 46);

// --- i64: Smi -> eager deopt "not a BigInt" ---
function test_i64_smi(arg) { return instance.exports.wasm_i64_arg(arg); }
%PrepareFunctionForOptimization(test_i64_smi);
test_i64_smi(1n);
test_i64_smi(1n);
%OptimizeFunctionOnNextCall(test_i64_smi);
// CHECK: [bailout (kind: deopt-eager, reason: not a BigInt): begin. deoptimizing {{.*}} <JSFunction test_i64_smi {{.*}}>
assertThrows(() => test_i64_smi(42), TypeError);

// --- i32 + externref return: original crash scenario (crbug.com/493307329) ---
// Valid arg: no deopt. The original crash involved an externref return type
// causing the deoptimizer to misread kReturnRegister0 as tagged.
function test_i32_extret(arg) {
  return instance.exports.wasm_i32_arg_extret(arg);
}
optimize_and_call(test_i32_extret, 5, null);
