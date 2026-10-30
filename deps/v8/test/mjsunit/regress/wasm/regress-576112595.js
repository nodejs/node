// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();

const s0 = builder.addStruct([makeField(kWasmI32, true)]);
const s1 = builder.addStruct({
  fields: [makeField(kWasmI32, true), makeField(kWasmI32, true)],
  supertype: s0,
});
const s2 = builder.addStruct({
  fields: [
    makeField(kWasmI32, true),
    makeField(kWasmI32, true),
    makeField(kWasmI32, true),
  ],
  supertype: s1,
  final: true,
});

const f = builder.addImport('m', 'f', kSig_v_i);

const sig = makeSig(
    [kWasmI32, wasmRefNullType(s0), wasmRefNullType(s0)],
    [kWasmI32]);

builder.addFunction('test', sig)
    .addLocals(wasmRefType(s1), 1)  // local 3: (ref $s1)
    .addBody([
      // local 3 = struct.new $s1 (10, 20)
      kExprI32Const, 10,
      kExprI32Const, 20,
      kGCPrefix, kExprStructNew, s1,
      kExprLocalSet, 3,
      kExprLoop, kWasmVoid,
        // call $f(ref.test (ref $s2) (local.get 3))
        kExprLocalGet, 3,
        kGCPrefix, kExprRefTest, s2,
        kExprCallFunction, f,
        // local 3 = ref.cast (ref $s1) (local.get 1)
        kExprLocalGet, 1,
        kGCPrefix, kExprRefCast, s1,
        kExprLocalSet, 3,
        // local 1 = local.get 2
        kExprLocalGet, 2,
        kExprLocalSet, 1,
        // br_if 0 (local.tee 0 (i32.sub (local.get 0) 1))
        kExprLocalGet, 0,
        kExprI32Const, 1,
        kExprI32Sub,
        kExprLocalTee, 0,
        kExprBrIf, 0,
      kExprEnd,
      kExprI32Const, 42,
    ])
    .exportFunc();

const instance = builder.instantiate({m: {f: () => {}}});
%WasmTierUpFunction(instance.exports.test);
