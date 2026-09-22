// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-debug-code

// Tests that WasmLoadElimination invalidates mutable state cache across a
// JSConstruct

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();

const struct_type = builder.addStruct([
  makeField(kWasmI32, true),
]);

builder.addFunction("get_field", makeSig([kWasmExternRef], [kWasmI32]))
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprAnyConvertExtern,
    kGCPrefix, kExprRefCast, struct_type,
    kGCPrefix, kExprStructGet, struct_type, 0,
  ])
  .exportFunc();

builder.addFunction("set_field", makeSig([kWasmExternRef, kWasmI32], []))
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprAnyConvertExtern,
    kGCPrefix, kExprRefCast, struct_type,
    kExprLocalGet, 1,
    kGCPrefix, kExprStructSet, struct_type, 0,
  ])
  .exportFunc();

builder.addFunction("make_struct", makeSig([kWasmI32], [kWasmExternRef]))
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprStructNew, struct_type,
    kGCPrefix, kExprExternConvertAny,
  ])
  .exportFunc();

const instance = builder.instantiate();
const { get_field, set_field, make_struct } = instance.exports;

// Three constructors to make JSConstruct megamorphic (prevents lowering to
// kCall).
function CtorA(ref) { set_field(ref, 1337); }
function CtorB(ref) { set_field(ref, 1337); }
function CtorC(ref) { set_field(ref, 1337); }
const ctors = [CtorA, CtorB, CtorC];

// The key function: TurboFan will inline get_field as WasmStructGet.
// The constructor parameter is polymorphic (CtorA/CtorB/CtorC), so
// JSConstruct won't be lowered to kCall by JSTypedLowering.
function vuln(ref, Ctor) {
  // WasmStructGet: cached.
  const val1 = get_field(ref);
  // JSConstruct (megamorphic): cache should be invalidated.
  const obj = new Ctor(ref);
  // WasmStructGet: Should not use stale cache.
  const val2 = get_field(ref);
  return [val1, val2];
}

// --- Deterministic optimization via V8 intrinsics ---
%PrepareFunctionForOptimization(vuln);
%PrepareFunctionForOptimization(CtorA);
%PrepareFunctionForOptimization(CtorB);
%PrepareFunctionForOptimization(CtorC);

// Warmup with all constructor variants for polymorphic feedback.
for (let i = 0; i < 10; i++) {
  const s = make_struct(i);
  vuln(s, ctors[i % 3]);
}

%OptimizeFunctionOnNextCall(vuln);

const test_struct = make_struct(42);
const [val1, val2] = vuln(test_struct, CtorA);

assertEquals(val2, get_field(test_struct));
