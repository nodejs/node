// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --nowasm-loop-unrolling --nowasm-loop-peeling --allow-natives-syntax

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();
const array = builder.addArray(kWasmI32, { mutability: true });
const struct = builder.addStruct([makeField(wasmRefType(array), true)]);

builder.addFunction('exploit', makeSig([], []))
  .addLocals(wasmRefType(array), 2) // big, small
  .addLocals(wasmRefType(struct), 1) // holder
  .addLocals(wasmRefType(array), 1) // cur
  .addLocals(kWasmI32, 1) // i
  .addBody([
    // Allocate arrays. 'big' (len 10), 'small' (len 1).
    kExprI32Const, 10,
    kGCPrefix, kExprArrayNewDefault, array,
    kExprLocalSet, 0,

    kExprI32Const, 1,
    kGCPrefix, kExprArrayNewDefault, array,
    kExprLocalSet, 1,

    // Allocate 'holder' struct initialized with 'big'.
    kExprLocalGet, 0,
    kGCPrefix, kExprStructNew, struct,
    kExprLocalSet, 2,

    // cur = big
    kExprLocalGet, 0,
    kExprLocalSet, 3,

    kExprI32Const, 5,
    kExprLocalSet, 4, // i = 5

    kExprLoop, kWasmVoid,
      // Optimized code incorrectly elides this bounds check in later iterations.
      // cur[3] = 0x1337
      kExprLocalGet, 3,
      kExprI32Const, 3,
      ...wasmI32Const(0x1337),
      kGCPrefix, kExprArraySet, array,

      // cur = holder.field (loads 'small' in 2nd iteration)
      kExprLocalGet, 2,
      kGCPrefix, kExprStructGet, struct, 0,
      kExprLocalSet, 3,

      // holder.field = small
      kExprLocalGet, 2,
      kExprLocalGet, 1,
      kGCPrefix, kExprStructSet, struct, 0,

      // if (--i > 0) br loop
      kExprLocalGet, 4,
      kExprI32Const, 1,
      kExprI32Sub,
      kExprLocalTee, 4,
      kExprI32Const, 0,
      kExprI32GtS,
      kExprBrIf, 0,
    kExprEnd,
  ])
  .exportFunc();

const instance = builder.instantiate({});
const wasm = instance.exports;

assertTraps(kTrapArrayOutOfBounds, () => wasm.exploit());
%WasmTierUpFunction(wasm.exploit);
assertTraps(kTrapArrayOutOfBounds, () => wasm.exploit());
