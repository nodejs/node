// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --jitless --wasm-jitless

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// memory.atomic.wait32/wait64/notify must depend on the sharedness of the
// accessed memory, not of memory 0, and a wait on a non-shared memory must
// throw the same error as the JIT tiers.

function build(mem0_shared, mem1_shared) {
  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 1, mem0_shared);
  builder.addMemory(1, 1, mem1_shared);
  for (const mem of [0, 1]) {
    builder.addFunction(`wait32_mem${mem}`, kSig_i_v)
        .addBody([
          kExprI32Const, 0,      // Address.
          kExprI32Const, 1,      // Expected value; the memory holds 0.
          ...wasmI64Const(0n),   // Timeout.
          kAtomicPrefix, kExprI32AtomicWait, 0x40 | 2, mem, 0,
        ])
        .exportFunc();
    builder.addFunction(`wait64_mem${mem}`, kSig_i_v)
        .addBody([
          kExprI32Const, 0,      // Address.
          ...wasmI64Const(1n),   // Expected value; the memory holds 0.
          ...wasmI64Const(0n),   // Timeout.
          kAtomicPrefix, kExprI64AtomicWait, 0x40 | 3, mem, 0,
        ])
        .exportFunc();
    builder.addFunction(`notify_mem${mem}`, kSig_i_v)
        .addBody([
          kExprI32Const, 0,  // Address.
          kExprI32Const, 1,  // Count.
          kAtomicPrefix, kExprAtomicNotify, 0x40 | 2, mem, 0,
        ])
        .exportFunc();
  }
  return builder.instantiate();
}

for (const [mem0_shared, mem1_shared] of
         [[false, false], [false, true], [true, false], [true, true]]) {
  const instance = build(mem0_shared, mem1_shared);
  [mem0_shared, mem1_shared].forEach((shared, mem) => {
    for (const op of ['wait32', 'wait64']) {
      const f = instance.exports[`${op}_mem${mem}`];
      if (shared) {
        // The value does not match, so the wait returns "not-equal" (1).
        assertEquals(1, f());
      } else {
        assertThrows(
            f, WebAssembly.RuntimeError,
            'Atomics.wait cannot be called in this context');
      }
    }
    assertEquals(0, instance.exports[`notify_mem${mem}`]());
  });
}
