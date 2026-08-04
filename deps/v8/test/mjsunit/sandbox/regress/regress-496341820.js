// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-memory-corruption-api --allow-natives-syntax
// Flags: --turbolev --turbolev-inline-js-wasm-wrappers

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();

// Wasm function 1 expects 20 f64 parameters This will cause an arity mismatch
// in the compiler if the JS call has 0 arguments.
let sig1 = builder.addType(makeSig(Array(20).fill(kWasmF64), []));
builder.addFunction("w1", sig1)
    .addBody([kExprNop])
    .exportAs("w1");

// Wasm function 2 expects 0 parameters. This is what the JS function will
// originally call.
let sig2 = builder.addType(makeSig([], []));
builder.addFunction("w2", sig2)
    .addBody([kExprNop])
    .exportAs("w2");

let instance = builder.instantiate();
let f1 = instance.exports.w1;
let f2 = instance.exports.w2;

// Create a view into the V8 sandbox memory
let buffer = new Sandbox.MemoryView(0, 0x100000000);
let memory = new DataView(buffer);

// Get the SharedFunctionInfo (SFI) object addresses for both exported Wasm
// functions.
let addr1 = Sandbox.getAddressOf(f1);
let addr2 = Sandbox.getAddressOf(f2);
// +16 is SFI offset, -1 to untag.
let sfi1 = memory.getUint32(addr1 + 16, true) - 1;
let sfi2 = memory.getUint32(addr2 + 16, true) - 1;

// Read the `trusted_function_data` pointer from both SFIs (+4 offset).
// These are the pointers we will swap.
let data1 = memory.getUint32(sfi1 + 4, true);
let data2 = memory.getUint32(sfi2 + 4, true);

function caller() {
    return f2();
}

%PrepareFunctionForOptimization(caller);
caller();

// We overwrite f2's `trusted_function_data` to point to f1's data.
// Thus the JavaScript call argument count will be different from the Wasm
// signature argument count and we won't get inlining anymore.
memory.setUint32(sfi2 + 4, data1, true);

// Trigger optimization. The background thread will compile `caller`.
%OptimizeFunctionOnNextCall(caller);

caller();
