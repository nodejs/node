// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-shared
d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
let shared_eqref = { opcode: 99, heap_type: -19, is_shared: true };

let struct_idx = builder.addStruct({
  fields: [
    makeField(kWasmI32, true),
    makeField(kWasmI64, true),
    makeField(shared_eqref, true)
  ],
  shared: true
});

let struct_ref = wasmRefType(struct_idx);

builder.addFunction("run_test", makeSig([], [kWasmI32]))
  .addLocals(struct_ref, 3) // s1, s2, parent
  .addBody([
    // s1 = create_struct(1, 10, null)
    kExprI32Const, 1,
    kExprI64Const, 10,
    kExprRefNull, kWasmSharedTypeForm, kEqRefCode,
    kGCPrefix, kExprStructNew, ...wasmUnsignedLeb(struct_idx),
    kExprLocalSet, 0,

    // s2 = create_struct(2, 20, null)
    kExprI32Const, 2,
    kExprI64Const, 20,
    kExprRefNull, kWasmSharedTypeForm, kEqRefCode,
    kGCPrefix, kExprStructNew, ...wasmUnsignedLeb(struct_idx),
    kExprLocalSet, 1,

    // parent = create_struct(0, 0, s1)
    kExprI32Const, 0,
    kExprI64Const, 0,
    kExprLocalGet, 0, // s1
    kGCPrefix, kExprStructNew, ...wasmUnsignedLeb(struct_idx),
    kExprLocalSet, 2,

    // Check 1: struct.atomic.get(parent) == s1
    kExprLocalGet, 2, // parent
    kAtomicPrefix, kExprStructAtomicGet, 0, ...wasmUnsignedLeb(struct_idx), 2,
    kExprLocalGet, 0, // s1
    kExprRefEq,
    kExprIf, kWasmI32,
      // Check 2: struct.atomic.rmw.xchg(parent, s2) == s1
      kExprLocalGet, 2, // parent
      kExprLocalGet, 1, // s2
      kAtomicPrefix, kExprStructAtomicExchange, 0, ...wasmUnsignedLeb(struct_idx), 2,
      kExprLocalGet, 0, // s1
      kExprRefEq,
      kExprIf, kWasmI32,
        // Check 3: struct.atomic.get(parent) == s2
        kExprLocalGet, 2, // parent
        kAtomicPrefix, kExprStructAtomicGet, 0, ...wasmUnsignedLeb(struct_idx), 2,
        kExprLocalGet, 1, // s2
        kExprRefEq,
        kExprIf, kWasmI32,
          // Check 4: struct.atomic.rmw.cmpxchg(parent, s2, s1) == s2
          kExprLocalGet, 2, // parent
          kExprLocalGet, 1, // s2 (expected)
          kExprLocalGet, 0, // s1 (new)
          kAtomicPrefix, kExprStructAtomicCompareExchange, 0, ...wasmUnsignedLeb(struct_idx), 2,
          kExprLocalGet, 1, // s2
          kExprRefEq,
          kExprIf, kWasmI32,
            // Check 5: struct.atomic.get(parent) == s1
            kExprLocalGet, 2, // parent
            kAtomicPrefix, kExprStructAtomicGet, 0, ...wasmUnsignedLeb(struct_idx), 2,
            kExprLocalGet, 0, // s1
            kExprRefEq,
          kExprElse,
            kExprI32Const, 0,
          kExprEnd,
        kExprElse,
          kExprI32Const, 0,
        kExprEnd,
      kExprElse,
        kExprI32Const, 0,
      kExprEnd,
    kExprElse,
      kExprI32Const, 0,
    kExprEnd,
  ]).exportFunc();

let instance = builder.instantiate();
assertEquals(1, instance.exports.run_test());
