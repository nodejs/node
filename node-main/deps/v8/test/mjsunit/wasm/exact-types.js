// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-custom-descriptors

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
let i32_field = makeField(kWasmI32, true);
let anyref = wasmRefNullType(kWasmAnyRef);

let $s0 = builder.addStruct({fields: [i32_field]});
let $s1 = builder.addStruct({fields: [i32_field], supertype: $s0});
let $s2 = builder.addStruct({fields: [i32_field], supertype: $s1});

let sig_r_v = makeSig([], [kWasmAnyRef]);
let sig_v_r = makeSig([kWasmAnyRef], []);
let sig_i_r = makeSig([kWasmAnyRef], [kWasmI32])

function MakeFunctions(type_index) {
  builder.addFunction("make_s" + type_index, sig_r_v).exportFunc().addBody([
    kGCPrefix, kExprStructNewDefault, type_index,
  ]);

  builder.addFunction("cast_s" + type_index, sig_v_r).exportFunc().addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprRefCast, kWasmExact, type_index,
    kExprDrop,
  ]);

  builder.addFunction("test_s" + type_index, sig_i_r).exportFunc().addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprRefTest, kWasmExact, type_index,
  ]);

  builder.addFunction("br_on_cast_exact_s" + type_index, sig_i_r)
    .exportFunc()
    .addBody([
      kExprBlock, kAnyRefCode,
        kExprLocalGet, 0,
        ...wasmBrOnCast(0, anyref, wasmRefNullType(type_index).exact()),
        kExprI32Const, 0, // If branch not taken, return 0.
        kExprReturn,
      kExprEnd,
      kExprDrop,
      kExprI32Const, 1, // If branch taken, return 1.
    ]);

  builder.addFunction("br_on_cast_fail_exact_s" + type_index, sig_i_r)
    .exportFunc()
    .addBody([
      kExprBlock, kAnyRefCode,
        kExprLocalGet, 0,
        ...wasmBrOnCastFail(0, anyref, wasmRefType(type_index).exact()),
        kExprI32Const, 0, // If branch not taken (cast succeeded), return 0.
        kExprReturn,
      kExprEnd,
      kExprDrop,
      kExprI32Const, 1, // If branch taken (cast failed), return 1.
    ]);
}

MakeFunctions($s0);
MakeFunctions($s1);
MakeFunctions($s2);

let instance = builder.instantiate();
let wasm = instance.exports;

let s0 = wasm.make_s0();
let s1 = wasm.make_s1();
let s2 = wasm.make_s2();

wasm.cast_s0(s0);
assertThrows(() => wasm.cast_s1(s0), WebAssembly.RuntimeError, "illegal cast");

assertThrows(() => wasm.cast_s0(s1), WebAssembly.RuntimeError, "illegal cast");
wasm.cast_s1(s1);
assertThrows(() => wasm.cast_s2(s1), WebAssembly.RuntimeError, "illegal cast");

assertThrows(() => wasm.cast_s1(s2), WebAssembly.RuntimeError, "illegal cast");
wasm.cast_s2(s2);

assertEquals(1, wasm.test_s0(s0));
assertEquals(0, wasm.test_s0(s1));
assertEquals(0, wasm.test_s0(s2));
assertEquals(0, wasm.test_s0(null));

assertEquals(0, wasm.test_s1(s0));
assertEquals(1, wasm.test_s1(s1));
assertEquals(0, wasm.test_s1(s2));
assertEquals(0, wasm.test_s1(null));

assertEquals(0, wasm.test_s2(s0));
assertEquals(0, wasm.test_s2(s1));
assertEquals(1, wasm.test_s2(s2));
assertEquals(0, wasm.test_s2(null));

assertEquals(1, wasm.br_on_cast_exact_s0(s0));
assertEquals(0, wasm.br_on_cast_exact_s0(s1));
assertEquals(0, wasm.br_on_cast_exact_s0(s2));
assertEquals(1, wasm.br_on_cast_exact_s0(null));

assertEquals(0, wasm.br_on_cast_exact_s1(s0));
assertEquals(1, wasm.br_on_cast_exact_s1(s1));
assertEquals(0, wasm.br_on_cast_exact_s1(s2));
assertEquals(1, wasm.br_on_cast_exact_s1(null));

assertEquals(0, wasm.br_on_cast_exact_s2(s0));
assertEquals(0, wasm.br_on_cast_exact_s2(s1));
assertEquals(1, wasm.br_on_cast_exact_s2(s2));
assertEquals(1, wasm.br_on_cast_exact_s2(null));

assertEquals(0, wasm.br_on_cast_fail_exact_s0(s0));
assertEquals(1, wasm.br_on_cast_fail_exact_s0(s1));
assertEquals(1, wasm.br_on_cast_fail_exact_s0(s2));
assertEquals(1, wasm.br_on_cast_fail_exact_s0(null));

assertEquals(1, wasm.br_on_cast_fail_exact_s1(s0));
assertEquals(0, wasm.br_on_cast_fail_exact_s1(s1));
assertEquals(1, wasm.br_on_cast_fail_exact_s1(s2));
assertEquals(1, wasm.br_on_cast_fail_exact_s1(null));

assertEquals(1, wasm.br_on_cast_fail_exact_s2(s0));
assertEquals(1, wasm.br_on_cast_fail_exact_s2(s1));
assertEquals(0, wasm.br_on_cast_fail_exact_s2(s2));
assertEquals(1, wasm.br_on_cast_fail_exact_s2(null));
