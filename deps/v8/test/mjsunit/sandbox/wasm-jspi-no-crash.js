// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/sandbox/wasm-jspi.js');

promise_f = new Promise(r => setTimeout(() => r([1, 1n, 1, 1, 1, 1n, 1, 1]), 0));
promise_g = new Promise(r => setTimeout(r, 0));

let suspend_f = new WebAssembly.Suspending(() => promise_f);
let suspend_g = new WebAssembly.Suspending(() => promise_g);
let builder = new WasmModuleBuilder();
let suspend_f_index = builder.addImport("m", "suspend_f", kSig_v_i);
let sig_g = builder.addType(makeSig(
    [],
    [kWasmI32, kWasmI64, kWasmF32, kWasmF64,
    kWasmI32, kWasmI64, kWasmF32, kWasmF64]));
let suspend_g_index = builder.addImport("m", "suspend_g", sig_g);
builder.addFunction("f", kSig_v_v).addBody([
    kExprI32Const, 0,
    kExprCallFunction, suspend_f_index,
]).exportFunc();
builder.addFunction("g", sig_g).addBody([
    kExprCallFunction, suspend_g_index,
]).exportFunc();

let instance = builder.instantiate({m:{suspend_f, suspend_g}});

let wasm_promise_f = WebAssembly.promising(instance.exports.f)();
let wasm_promise_g = WebAssembly.promising(instance.exports.g)();

// Swap the WasmResumeData's suspenders. We don't expect any crashes: the stacks
// are just resumed in the wrong order.
let suspender_f = get_suspender(get_resume_data(promise_f));
set_suspender(
    get_resume_data(promise_f),
    get_suspender(get_resume_data(promise_g)));
set_suspender(get_resume_data(promise_g), suspender_f);

// Results are swapped!
assertPromiseResult(wasm_promise_f, v => assertEquals(undefined, v));
assertPromiseResult(
    wasm_promise_g,
    v => assertEquals([1, 1n, 1, 1, 1, 1n, 1, 1], v));
