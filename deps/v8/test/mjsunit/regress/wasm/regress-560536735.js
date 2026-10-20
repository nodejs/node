// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
let $dummy = builder.addFunction('dummy', kSig_v_v).addBody([]);
let $t = builder.addType(makeSig(Array(8).fill(kWasmI64), []));
let $func = builder.addFunction("func", $t).addBody([
  kExprI32Const, 0,
  kExprIf, kWasmVoid,
    // An unreachable call nevertheless requires a feedback vector entry.
    kExprCallFunction, $dummy.index,
  kExprEnd,
]);
builder.addFunction("main", kSig_v_i).exportFunc().addBody([
  kExprLocalGet, 0,
  kExprI32Const, 0,
  kExprI32Eq,
  kExprIf, kWasmVoid,
    ...Array(8).fill(wasmI64Const(0x424242424241n)).flat(),
    // A tail call means that the LiftoffFrameSetup builtin will appear to
    // have been called directly by the JS function.
    kExprReturnCall, $func.index,
  kExprEnd,
]);

let instance = builder.instantiate();
let main = instance.exports.main;

// We need a JS function that inlines the JS-to-Wasm wrapper.
function call_main(i) {
  return main(i);
}
%PrepareFunctionForOptimization(call_main);
for (let i = 0; i < 10; i++) call_main(1);
%OptimizeFunctionOnNextCall(call_main);
call_main(1);

// Passing 0 to call_main causes "func" to get called, and therefore allocate
// its feedback vector. A full newspace makes this allocation trigger GC.
%SimulateNewspaceFull();
call_main(0);
