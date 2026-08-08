// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-wasmfx

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function TestSwitchCoroutineLoop() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  // Define a mutually recursive type group:
  // sig_coro: (i32, ref cont_coro) -> (i32)
  // cont_coro: continuation of sig_coro
  builder.startRecGroup();
  let cont_coro_idx = builder.nextTypeIndex() + 1;
  let sig_coro_idx = builder.addType(makeSig([kWasmI32, wasmRefType(cont_coro_idx)], [kWasmI32]));
  /* cont_coro_ix */ builder.addCont(sig_coro_idx);
  builder.endRecGroup();

  let tag_switch = builder.addTag(kSig_i_v);

  // The coroutine function
  let coro_func = builder.addFunction("coro_func", sig_coro_idx)
      .addBody([
          kExprBlock, kWasmVoid,
            kExprLoop, kWasmVoid,
              // Check if counter == 0, if so, break out of block (to return)
              kExprLocalGet, 0,
              kExprI32Eqz,
              kExprBrIf, 1,

              // Decrement counter
              kExprLocalGet, 0,
              kExprI32Const, 1,
              kExprI32Sub,

              // Push target continuation
              kExprLocalGet, 1,

              // Switch! This suspends the current stack, packages it into a
              // new continuation, and passes (counter-1, new_cont) to the
              // target continuation.
              kExprSwitch, cont_coro_idx, tag_switch,

              // When the *other* coroutine switches back to us, it passes
              // (counter, its_continuation) back. These are on top of the
              // stack now. Update our locals!
              // The signature parameters are (i32, ref cont_coro), so the
              // stack has: [..., i32, ref cont_coro]
              kExprLocalSet, 1,
              kExprLocalSet, 0,

              // Loop again
              kExprBr, 0,
            kExprEnd,
          kExprEnd,
          // Return the final counter value (should be 0)
          kExprLocalGet, 0,
      ]).exportFunc();

  builder.addFunction("main", kSig_i_v)
      .addBody([
          // Initial counter value
          ...wasmI32Const(100000),

          // Create contB (the "other" coroutine)
          kExprRefFunc, coro_func.index,
          kExprContNew, cont_coro_idx,

          // Create contA (the first coroutine)
          kExprRefFunc, coro_func.index,
          kExprContNew, cont_coro_idx,

          // Resume contA, passing the counter and contB
          kExprResume, cont_coro_idx, 1,
            kOnSwitch, tag_switch,
      ]).exportFunc();

  let instance = builder.instantiate();
  assertEquals(0, instance.exports.main());
})();
