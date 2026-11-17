// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-custom-descriptors

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();

builder.startRecGroup();
let $desc0 = builder.nextTypeIndex() + 1;
let $s0 = builder.addStruct({descriptor: $desc0});
builder.addStruct({describes: $s0});
builder.endRecGroup();

builder.startRecGroup();
let $desc1 = builder.nextTypeIndex() + 1;
let $s1 = builder.addStruct({descriptor: $desc1});
builder.addStruct({describes: $s1});
builder.endRecGroup();

let ref_exact_s0 = wasmRefType($s0).exact();
let ref_exact_s1 = wasmRefType($s1).exact();
let ref_exact_desc1 = wasmRefType($desc1).exact();

// Direct stack flow: ref.get_desc $s1 on a value of type (ref (exact $s0))
// must preserve exactness and produce (ref (exact $desc1)).
builder.addFunction("direct", makeSig([ref_exact_s0], [ref_exact_desc1]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprRefGetDesc, $s1,
    ]);

// Flow through a local of type (ref (exact $s1)) into ref.get_desc $s1 and
// ref.cast_desc_eq (exact $s1), exercising WasmGCTypeAnalyzer::ProcessStructGet.
builder.addFunction("via_local", makeSig([ref_exact_s0], [kWasmAnyRef]))
    .addLocals(ref_exact_s1, 1)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprLocalTee, 1,
      kExprLocalGet, 1,
      kGCPrefix, kExprRefGetDesc, $s1,
      kGCPrefix, kExprRefCastDescEq, kWasmExact, $s1,
    ]);

builder.addFunction("make_s0", makeSig([], [ref_exact_s0]))
    .exportFunc()
    .addBody([
      kGCPrefix, kExprStructNewDefault, $desc0,
      kGCPrefix, kExprStructNewDefaultDesc, $s0,
    ]);

let wasm = builder.instantiate().exports;
let s0 = wasm.make_s0();
assertSame(s0, wasm.via_local(s0));
assertSame(wasm.direct(s0), wasm.direct(s0));
