// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-wasmfx --expose-gc

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function TestMinorGCOnSuspendedStack() {
  let builder = new WasmModuleBuilder();
  let struct_type = builder.addStruct([makeField(kWasmI32, true)]);
  let struct_ref = wasmRefType(struct_type);

  let sig_i_v = builder.addType(kSig_i_v);
  let cont_i_v = builder.addCont(sig_i_v);

  let tag0 = builder.addTag(kSig_v_v);

  let block_sig = builder.addType(makeSig([], [wasmRefType(cont_i_v)]));
  let g_cont = builder.addGlobal(wasmRefNullType(cont_i_v), true, false);

  // Inner function: allocates a struct, stores it in a local, suspends, and reads it after resume.
  let inner = builder.addFunction("inner", sig_i_v)
      .addLocals(struct_ref, 1)
      .addBody([
        // Allocate a new struct with value 42 and spill it to local 0.
        ...wasmI32Const(42),
        kGCPrefix, kExprStructNew, struct_type,
        kExprLocalSet, 0,

        // Suspend the stack.
        kExprSuspend, tag0,

        // Verify the struct reference in local 0 is still valid.
        kExprLocalGet, 0,
        kGCPrefix, kExprStructGet, struct_type, 0,
      ]);

  // Outer function on continuation: also allocates a struct and calls inner.
  let outer = builder.addFunction("outer", sig_i_v)
      .addLocals(struct_ref, 1)
      .addBody([
        // Allocate a struct with value 100 in local 0.
        ...wasmI32Const(100),
        kGCPrefix, kExprStructNew, struct_type,
        kExprLocalSet, 0,

        // Call inner, which will suspend.
        kExprCallFunction, inner.index,

        // Add the value from local 0 (100) to inner's return value (42).
        kExprLocalGet, 0,
        kGCPrefix, kExprStructGet, struct_type, 0,
        kExprI32Add,
      ]);

  builder.addDeclarativeElementSegment([outer.index]);

  // Start the continuation and suspend.
  builder.addFunction("start", kSig_v_v)
      .addBody([
        kExprBlock, block_sig,
          kExprRefFunc, outer.index,
          kExprContNew, cont_i_v,
          kExprResume, cont_i_v, 1, kOnSuspend, tag0, 0,
          // If outer returns without suspending, unreachable.
          kExprUnreachable,
        kExprEnd,
        // Save suspended continuation in global.
        kExprGlobalSet, g_cont.index,
      ]).exportFunc();

  // Resume the suspended continuation.
  builder.addFunction("resume", sig_i_v)
      .addBody([
        kExprGlobalGet, g_cont.index,
        kExprResume, cont_i_v, 0,
      ]).exportFunc();

  let instance = builder.instantiate();

  // 1. Start the continuation; it will suspend in inner().
  instance.exports.start();

  // 2. Trigger first minor GC: struct objects survive and copy to to-space.
  gc({ type: 'minor' });

  // 3. Trigger second minor GC: struct objects are promoted to old space,
  // setting skip_minor_gc_ = true.
  gc({ type: 'minor' });

  // 4. Trigger third minor GC: takes the early return path (skipping the stack).
  gc({ type: 'minor' });

  // 5. Resume the continuation and verify the spilled struct objects were preserved.
  let result = instance.exports.resume();
  assertEquals(142, result);
})();
