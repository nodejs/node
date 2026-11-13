// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --trace-wasm-typer --trace-turbo-graph --no-liftoff
// Flags: --no-wasm-lazy-compilation --no-wasm-assert-types

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const Box = 0;
const Arr = 1;
const sigBoxOrI31 =
    makeSig([kWasmI32, wasmRefType(Box), wasmRefType(kWasmI31Ref)], [kWasmI32]);

function compile(name, sig, body, locals = []) {
  let builder = new WasmModuleBuilder();
  builder.addStruct([makeField(kWasmI32, false)]);
  builder.addArray(kWasmI32);
  let fn = builder.addFunction(name, sig);
  for (let l of locals) fn.addLocals(l, 1);
  fn.addBody(body);
  builder.instantiate();
}

// CHECK-LABEL: Begin compiling method checkI31FirstBrOnCast using Turboshaft
// CHECK: Refine type for object #[[PHI1:[0-9]+]](Phi) -> (ref 0)|i31
// CHECK: Branch): Refine type for object #[[PHI1]](Phi) -> (ref i31)
// CHECK: Branch): Refine type for object #[[PHI1]](Phi) -> (ref 0)
// CHECK-LABEL: V8.TFTurboshaftWasmGCOptimize
// CHECK-NOT: WasmTypeCast(
// CHECK: Return(
compile('checkI31FirstBrOnCast', sigBoxOrI31, [
  kExprBlock, kI31RefCode,
    kExprLocalGet, 0,
    kExprIf, kEqRefCode,
      kExprLocalGet, 1,
    kExprElse,
      kExprLocalGet, 2,
    kExprEnd,
    kGCPrefix, kExprBrOnCast, 0b01, 0, kEqRefCode, kI31RefCode,
    kGCPrefix, kExprRefCast, Box,
    kGCPrefix, kExprStructGet, Box, 0,
    kExprReturn,
  kExprEnd,
  kGCPrefix, kExprI31GetS,
]);

// CHECK-LABEL: Begin compiling method checkStructFirstBrOnCastFail using Turboshaft
// CHECK: Refine type for object #[[PHI2:[0-9]+]](Phi) -> (ref 0)|i31
// CHECK: Branch): Refine type for object #[[PHI2]](Phi) -> (ref i31)
// CHECK: Branch): Refine type for object #[[PHI2]](Phi) -> (ref 0)
// CHECK-LABEL: V8.TFTurboshaftWasmGCOptimize
// CHECK-NOT: WasmTypeCast(
// CHECK: Return(
compile('checkStructFirstBrOnCastFail', sigBoxOrI31, [
  kExprBlock, kEqRefCode,
    kExprLocalGet, 0,
    kExprIf, kEqRefCode,
      kExprLocalGet, 1,
    kExprElse,
      kExprLocalGet, 2,
    kExprEnd,
    kGCPrefix, kExprBrOnCastFail, 0b01, 0, kEqRefCode, Box,
    kGCPrefix, kExprStructGet, Box, 0,
    kExprReturn,
  kExprEnd,
  kGCPrefix, kExprRefCast, kI31RefCode,
  kGCPrefix, kExprI31GetS,
]);

// CHECK-LABEL: Begin compiling method castStructOrArray using Turboshaft
// CHECK: Refine type for object #[[PHI3:[0-9]+]](Phi) -> (ref eq)∖i31
// CHECK-LABEL: V8.TFTurboshaftWasmGCOptimize
// CHECK-NOT: WasmTypeCheck(
// CHECK: WasmTypeCast(#[[PHI3]], {{.*}})[(ref eq)∖i31 -> (ref 0)
compile('castStructOrArray',
    makeSig([kWasmI32, wasmRefType(Box), wasmRefType(Arr)], [wasmRefType(Box)]),
    [
      kExprBlock, kAnyRefCode,
        kExprLocalGet, 0,
        kExprIf, kEqRefCode,
          kExprLocalGet, 1,
        kExprElse,
          kExprLocalGet, 2,
        kExprEnd,
        kGCPrefix, kExprBrOnCast, 0b01, 0, kEqRefCode, kI31RefCode,
        kGCPrefix, kExprRefCast, Box,
        kExprReturn,
      kExprEnd,
      kExprUnreachable,
    ]);

// CHECK-LABEL: Begin compiling method narrowI31RefToNull using Turboshaft
// CHECK: Branch): Refine type for object #[[P0:[0-9]+]](Parameter) -> (ref i31)
// CHECK: Branch): Refine type for object #[[P0]](Parameter) -> nullref
// CHECK-LABEL: V8.TFTurboshaftWasmGCOptimize
// CHECK-NOT: WasmTypeCheck(
// CHECK-NOT: WasmTypeCast(
// CHECK: IsNull(#[[P0]])[i31ref
// CHECK-NOT: IsNull(
// CHECK: Return(
compile('narrowI31RefToNull', makeSig([kWasmI31Ref], [kWasmI32]), [
  // Widen static type to eqref via local 1 so the decoder emits WasmTypeCheck
  // instead of folding at decode time.
  kExprLocalGet, 0, kExprLocalTee, 1,
  kGCPrefix, kExprRefTest, kI31RefCode,
  kExprIf, kWasmI32,
    kExprLocalGet, 1, kGCPrefix, kExprRefCast, kI31RefCode,
    kGCPrefix, kExprI31GetS,
  kExprElse,
    // local 1 is narrowed to nullref: ref.is_null folds to 1 and non-nullable
    // ref.test (ref $Box) folds to 0 without emitting IsNull or WasmTypeCheck.
    kExprLocalGet, 1, kExprRefIsNull,
    kExprLocalGet, 1, kGCPrefix, kExprRefTest, Box,
    kExprI32Sub,
  kExprEnd,
], [kWasmEqRef]);

// CHECK-LABEL: Begin compiling method refTestNullOnNullableStruct using Turboshaft
// CHECK-LABEL: V8.TFTurboshaftWasmGCOptimize
// CHECK-NOT: WasmTypeCheck(
// CHECK: IsNull(#{{[0-9]+}})[(ref null 0)
// CHECK: Return(
compile('refTestNullOnNullableStruct',
    makeSig([wasmRefNullType(Box)], [kWasmI32]), [
      // ref.test nullref on (ref null $Box) folds to IsNull via
      // HeapTypesUnrelated.
      kExprLocalGet, 0, kGCPrefix, kExprRefTestNull, kNullRefCode,
    ]);
