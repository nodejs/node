// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
// Flags: --turbolev --maglev
// Flags: --wasm-in-js-inlining-wrapper --wasm-in-js-inlining-body

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const f = builder.addFunction('f', kSig_v_v).addBody([]);
builder.addDeclarativeElementSegment([f.index]);

builder.addFunction('wasm_warmup', kSig_i_v)
    .addBody([kExprI32Const, 7])
    .exportFunc();
builder.addFunction('wasm_main', makeSig([kWasmExternRef], [kWasmI32]))
    .addBody([
      kExprLocalGet, 0,
      // See below why we need this `ref.as_non_null` to "return" before
      // reaching the `ref.func` in the non-optimized case.
      kExprRefAsNonNull, kExprDrop,
      kExprRefFunc, f.index,
      // We unfortunately cannot return the funcref directly to JavaScript,
      // since our inlining doesn't support that type at the
      kExprRefIsNull
    ])
    .exportFunc();

const instance = builder.instantiate();
const wasm_warmup = instance.exports.wasm_warmup;
const wasm_main = instance.exports.wasm_main;

function test(b, arg) {
  if (b) return wasm_warmup();
  return wasm_main(arg);
}

%PrepareFunctionForOptimization(test);

// 1. Warmup: We call `wasm_warmup()` 20 times to cleanly increment `test`'s
// invocation counter for JIT tiering without contaminating the feedback vector
// with exception records (which would happen if `wasm_main(null)` were called).
for (let i = 0; i < 20; i++) test(true, null);

// 2. Pre-Optimization IC population: We call `wasm_main(null)` exactly once.
// This populates the monomorphic IC feedback slot for `wasm_main` so Turboshaft
// inlines it. Because `arg` is `null`, `ref.as_non_null` traps immediately,
// exiting the function before `ref.func f.index` is reached. Thus, `f` remains
// uninitialized in the instance's `func_refs` cache.
assertTraps(kTrapNullDereference, () => test(false, null));

%OptimizeFunctionOnNextCall(test);

// 3. Optimized Execution: Calling `test()` (where `b` is `undefined`, so falsy)
// executes inlined `wasm_main()`. In optimized code, `arg` is `undefined`
// (which Wasm considers a non-null externref), passing `ref.as_non_null` and
// reaching the uninitialized `ref.func`. This triggers the slow-path builtin
// call `WasmRefFunc` from the JS frame, exposing the instance lookup mismatch.
const result = test();
// Make sure that `test` wasn't deoptimized.
assertOptimized(test);
assertEquals(0, result);
