// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --nowasm-loop-unrolling --nowasm-loop-peeling

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();

// struct Inner { y: i32 }
const innerStruct = builder.addStruct([makeField(kWasmI32, true)]);

// struct Outer { x: ref Inner }
const outerStruct = builder.addStruct([makeField(wasmRefType(innerStruct), true)]);

// function foo(c: i32, o: ref Outer, other: ref Inner) -> i32
builder.addFunction(
    'foo',
    makeSig([kWasmI32, wasmRefType(outerStruct), wasmRefType(innerStruct)],
            [kWasmI32]))
  .addLocals(wasmRefType(innerStruct), 1) // 3: base
  .addLocals(kWasmI32, 1)                 // 4: initial_y
  .addLocals(kWasmI32, 1)                 // 5: ret
  .addLocals(wasmRefType(innerStruct), 1) // 6: loop_phi
  .addLocals(kWasmI32, 1)                 // 7: i
  .addLocals(wasmRefType(innerStruct), 1) // 8: inner_phi
  .addBody([
    // let base = o.x;
    kExprLocalGet, 1, // o
    kGCPrefix, kExprStructGet, outerStruct, 0,
    kExprLocalSet, 3, // base

    // let initial_y = base.y;
    kExprLocalGet, 3, // base
    kGCPrefix, kExprStructGet, innerStruct, 0,
    kExprLocalSet, 4, // initial_y

    // let ret = initial_y;
    kExprLocalGet, 4, // initial_y
    kExprLocalSet, 5, // ret

    // let loop_phi = base;
    kExprLocalGet, 3, // base
    kExprLocalSet, 6, // loop_phi

    // i = 0
    kExprI32Const, 0,
    kExprLocalSet, 7, // i

    // loop
    kExprLoop, kWasmVoid,
      // let inner_phi = c ? o.x : base;
      kExprLocalGet, 0, // c
      kExprIf, kWasmRef, innerStruct,
        kExprLocalGet, 1, // o
        kGCPrefix, kExprStructGet, outerStruct, 0,
      kExprElse,
        kExprLocalGet, 3, // base
      kExprEnd,
      kExprLocalSet, 8, // inner_phi

      // ret = loop_phi.y;
      kExprLocalGet, 6, // loop_phi
      kGCPrefix, kExprStructGet, innerStruct, 0,
      kExprLocalSet, 5, // ret

      // o.x = other;
      kExprLocalGet, 1, // o
      kExprLocalGet, 2, // other
      kGCPrefix, kExprStructSet, outerStruct, 0,

      // loop_phi = inner_phi;
      kExprLocalGet, 8, // inner_phi
      kExprLocalSet, 6, // loop_phi

      // i++
      kExprLocalGet, 7, // i
      kExprI32Const, 1,
      kExprI32Add,
      kExprLocalTee, 7, // i
      kExprI32Const, 42,
      kExprI32LtS,
      kExprBrIf, 0,
    kExprEnd,

    // return ret;
    kExprLocalGet, 5, // ret
  ])
  .exportFunc();

builder.addFunction(
    'create_inner', makeSig([kWasmI32], [wasmRefType(innerStruct)]))
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprStructNew, innerStruct,
  ])
  .exportFunc();

builder.addFunction(
    'create_outer',
    makeSig([wasmRefType(innerStruct)], [wasmRefType(outerStruct)]))
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprStructNew, outerStruct,
  ])
  .exportFunc();

const instance = builder.instantiate({});
const wasm = instance.exports;

let o1_inner = wasm.create_inner(17);
let o1 = wasm.create_outer(o1_inner);
let o2 = wasm.create_inner(29);

let res = wasm.foo(1, o1, o2);
assertEquals(29, res);
