// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-wasm-loop-unrolling

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();

let struct_type = builder.addStruct([makeField(kWasmI32, true)]);

builder.addFunction("foo", kSig_i_v)
  .addLocals(kWasmI32, 4) // phi1, phi2, phi3, i
  .addLocals(wasmRefNullType(struct_type), 1) // gc_struct
  .addBody([
    // phi1 = 0
    kExprI32Const, 0,
    kExprLocalSet, 0,
    // phi3 = 0
    kExprI32Const, 0,
    kExprLocalSet, 2,
    // i = 0
    kExprI32Const, 0,
    kExprLocalSet, 3,

    // Allocate struct before loop
    kExprI32Const, 0,
    kGCPrefix, kExprStructNew, struct_type,
    kExprLocalSet, 4,

    // phi2 = gc_struct.field[0] (initializes phi2 using GC)
    kExprLocalGet, 4,
    kGCPrefix, kExprStructGet, struct_type, 0,
    kExprLocalSet, 1,

    // Block to exit loop
    kExprBlock, kWasmVoid,
      // Loop
      kExprLoop, kWasmVoid,
        // if i >= 42, branch to block (exit loop)
        kExprLocalGet, 3,
        kExprI32Const, 42,
        kExprI32GeS,
        kExprBrIf, 1, // 1 refers to outer block

        // loop body
        // phi1 = phi2
        kExprLocalGet, 1,
        kExprLocalSet, 0,

        // phi2 = 0 (simple assignment, no GC)
        kExprI32Const, 0,
        kExprLocalSet, 1,

        // phi3 = phi1
        kExprLocalGet, 0,
        kExprLocalSet, 2,

        // i++
        kExprLocalGet, 3,
        kExprI32Const, 1,
        kExprI32Add,
        kExprLocalSet, 3,

        // branch to loop start
        kExprBr, 0,
      kExprEnd, // end loop
    kExprEnd, // end block

    // return phi3
    kExprLocalGet, 2
  ])
  .exportFunc();

const instance = builder.instantiate();
const foo = instance.exports.foo;

foo();

%WasmTierUpFunction(foo);

foo();
