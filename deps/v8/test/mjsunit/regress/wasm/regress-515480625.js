// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-liftoff --no-wasm-loop-unrolling

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();

let struct_type = builder.addStruct([makeField(kWasmI32, true)]);

builder.addFunction('make_o', makeSig([kWasmI32], [wasmRefType(struct_type)]))
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprStructNew, struct_type,
  ])
  .exportFunc();

let foo = builder.addFunction('foo', makeSig([wasmRefType(struct_type)], [kWasmI32]))
  .addLocals(kWasmI32, 4) // base (1), phi1 (2), phi2 (3), i (4)
  .addBody([
    // base = o.x
    kExprLocalGet, 0,
    kGCPrefix, kExprStructGet, struct_type, 0,
    kExprLocalSet, 1,

    // Creating 2 loop phis where
    //   - The 2nd one is the backedge of the 1st one (the order matters!)
    //   - The 2nd one's backedge is identical as its forward edge.
    //
    // This means that {phi2} can be replaced by {base}, AFTER WHICH {phi1} can be
    // replaced by {base} as well. If we visit the loop header only once, then
    // only {phi2} gets replaced, whereas if we visit it twice in a row, then
    // during the 1st visit {phi2} gets replaced and during the 2nd one {phi1}
    // gets replaced as well.

    // phi1 = o.x
    kExprLocalGet, 0,
    kGCPrefix, kExprStructGet, struct_type, 0,
    kExprLocalSet, 2,

    // phi2 = o.x
    kExprLocalGet, 0,
    kGCPrefix, kExprStructGet, struct_type, 0,
    kExprLocalSet, 3,

    // i = 0
    kExprI32Const, 0,
    kExprLocalSet, 4,

    // loop
    kExprBlock, kWasmVoid,
      kExprLoop, kWasmVoid,
        // if i >= 42 break
        kExprLocalGet, 4,
        kExprI32Const, 42,
        kExprI32GeS,
        kExprBrIf, 1,

        // phi1 = phi2
        kExprLocalGet, 3,
        kExprLocalSet, 2,

        // phi2 = o.x
        kExprLocalGet, 0,
        kGCPrefix, kExprStructGet, struct_type, 0,
        kExprLocalSet, 3,

        // i++
        kExprLocalGet, 4,
        kExprI32Const, 1,
        kExprI32Add,
        kExprLocalSet, 4,

        // loop again
        kExprBr, 0,
      kExprEnd,
    kExprEnd,

    // return phi1
    kExprLocalGet, 2,
  ])
  .exportFunc();

let instance = builder.instantiate({});
let wasm = instance.exports;

// Create struct o with x = 123
let o = wasm.make_o(123);

assertEquals(123, wasm.foo(o));
