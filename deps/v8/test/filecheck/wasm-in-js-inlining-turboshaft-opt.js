// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Only compile and optimize via Turboshaft to ensure stable traces.
// Flags: --allow-natives-syntax --turbofan
// Flags: --turbolev --wasm-in-js-inlining-wrapper --wasm-in-js-inlining-body --wasm-in-js-inlining-opt

// Disable V8 stress modes and baseline compilers to guarantee deterministic compilation.
// Flags: --no-always-sparkplug --no-stress-maglev --no-stress-concurrent-inlining
// Flags: --trace-turbo-inlining

// Tracing for optimizations (Wasm GC type analyzer & Turboshaft reductions).
// Flags: --trace-wasm-typer --turboshaft-trace-reduction --turbo-filter=js_*

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");
d8.file.execute("test/mjsunit/mjsunit.js");

function testOptimized(jsFn, ...testCases) {
  const run = () => {
    for (const [expected, ...args] of testCases) {
      assertEquals(expected, jsFn(...args));
    }
  };
  %PrepareFunctionForOptimization(jsFn);
  for (let i = 0; i < 10; ++i) run();
  %OptimizeFunctionOnNextCall(jsFn);
  run();
  assertOptimized(jsFn);
}

const builder = new WasmModuleBuilder();
const array = builder.addArray(kWasmI32);
const globalArray = builder.addGlobal(wasmRefNullType(array), true, false);

builder.addFunction('initGlobalArray', makeSig([], [])).addBody([
  ...wasmI32Const(42),
  kGCPrefix, kExprArrayNewDefault, array,
  kExprGlobalSet, globalArray.index,
]).exportFunc();

// =============================================================================
// Wasm Function Definitions
// =============================================================================

// WasmGCTypedOptimizationReducer

builder.addFunction('redundantRefCast', makeSig([kWasmExternRef], [kWasmI32])).addBody([
  kExprLocalGet, 0,
  kGCPrefix, kExprAnyConvertExtern,
  kExprLocalSet, 1,
  // 1. First ref.cast (non-null): emits HeapConstant(Map) + WasmTypeCast.
  kExprLocalGet, 1,
  kGCPrefix, kExprRefCast, array,
  kExprDrop,
  // 2. Second identical ref.cast (non-null): because the RTT Map is baked in as
  //    a HeapConstant, ValueNumberingReducer (GVN) in WasmInJSInliningPhase
  //    deduplicates both the RTT constant and this identical WasmTypeCast
  //    on-the-fly during inlining.
  kExprLocalGet, 1,
  kGCPrefix, kExprRefCast, array,
  kExprDrop,
  // 3. Third ref.cast null (nullable): has a different target type
  //    (ref null $array), so GVN in WasmInJSInliningPhase does not fold it
  //    (though it still shares the GVN'd RTT HeapConstant), leaving the cast
  //    for WasmGCTypedOptimizationReducer to eliminate in WasmGCOptimizePhase.
  kExprLocalGet, 1,
  kGCPrefix, kExprRefCastNull, array,
  kGCPrefix, kExprArrayLen,
]).addLocals(kWasmAnyRef, 1).exportFunc();

builder.addFunction('redundantConversion', makeSig([kWasmExternRef], [kWasmI32])).addBody([
  kExprLocalGet, 0,
  kGCPrefix, kExprAnyConvertExtern,
  kGCPrefix, kExprExternConvertAny,
  kGCPrefix, kExprAnyConvertExtern,
  kGCPrefix, kExprRefCastNull, array,
  kGCPrefix, kExprArrayLen,
]).exportFunc();

builder.addFunction('redundantIsNull', makeSig([kWasmExternRef], [kWasmI32])).addBody([
  kExprLocalGet, 0,
  kGCPrefix, kExprAnyConvertExtern,
  kExprLocalSet, 1,
  kExprLocalGet, 1,
  kGCPrefix, kExprRefCast, array,
  kExprDrop,
  kExprLocalGet, 1,
  kExprRefIsNull,
]).addLocals(kWasmAnyRef, 1).exportFunc();

const dummyFunc = builder.addFunction('dummyFunc', kSig_v_v).addBody([]);
builder.addDeclarativeElementSegment([dummyFunc.index]);
builder.addFunction('constantFunctionRefIsNull', makeSig([], [kWasmI32])).addBody([
  kExprRefFunc, dummyFunc.index,
  kExprRefIsNull,
]).exportFunc();


// WasmLoadEliminationReducer

const struct = builder.addStruct([makeField(kWasmI32, true)]);

builder.addFunction('redundantStructGet', makeSig([kWasmExternRef], [kWasmI32])).addBody([
  kExprLocalGet, 0,
  kGCPrefix, kExprAnyConvertExtern,
  kGCPrefix, kExprRefCast, struct,
  kExprLocalSet, 1,
  kExprLocalGet, 1,
  kGCPrefix, kExprStructGet, struct, 0,
  kExprLocalGet, 1,
  kGCPrefix, kExprStructGet, struct, 0,
  kExprI32Add,
]).addLocals(wasmRefNullType(struct), 1).exportFunc();

builder.addFunction('structGetOnce', makeSig([kWasmExternRef], [kWasmI32])).addBody([
  kExprLocalGet, 0,
  kGCPrefix, kExprAnyConvertExtern,
  kGCPrefix, kExprRefCast, struct,
  kGCPrefix, kExprStructGet, struct, 0,
]).exportFunc();

// Helper to allocate struct as externref
builder.addFunction('newStructAsExternref', makeSig([kWasmI32], [kWasmExternRef])).addBody([
  kExprLocalGet, 0,
  kGCPrefix, kExprStructNew, struct,
  kGCPrefix, kExprExternConvertAny,
]).exportFunc();

builder.addFunction('storeToLoadForwarding', makeSig([kWasmExternRef, kWasmI32], [kWasmI32])).addBody([
  kExprLocalGet, 0,
  kGCPrefix, kExprAnyConvertExtern,
  kGCPrefix, kExprRefCast, struct,
  kExprLocalSet, 2,
  kExprLocalGet, 2,
  kExprLocalGet, 1,
  kGCPrefix, kExprStructSet, struct, 0,
  kExprLocalGet, 2,
  kGCPrefix, kExprStructGet, struct, 0,
]).addLocals(wasmRefNullType(struct), 1).exportFunc();

builder.addFunction('aliasingBoundsOnStore', makeSig([kWasmExternRef, kWasmExternRef], [kWasmI32])).addBody([
  kExprLocalGet, 0,
  kGCPrefix, kExprAnyConvertExtern,
  kGCPrefix, kExprRefCast, struct,
  kExprLocalSet, 2,
  kExprLocalGet, 1,
  kGCPrefix, kExprAnyConvertExtern,
  kGCPrefix, kExprRefCast, struct,
  kExprLocalSet, 3,
  kExprLocalGet, 2,
  kGCPrefix, kExprStructGet, struct, 0,
  kExprDrop,
  kExprLocalGet, 2,
  kExprI32Const, 55,
  kGCPrefix, kExprStructSet, struct, 0,
  kExprLocalGet, 2,
  kGCPrefix, kExprStructGet, struct, 0,
  kExprDrop,
  kExprLocalGet, 3,
  kExprI32Const, 42,
  kGCPrefix, kExprStructSet, struct, 0,
  kExprLocalGet, 2,
  kGCPrefix, kExprStructGet, struct, 0,
]).addLocals(wasmRefNullType(struct), 2).exportFunc();

const structA = builder.addStruct([makeField(kWasmI32, true)]);
const structB = builder.addStruct([makeField(kWasmF32, true)]);

for (const [name, opcode] of [
  ['refTestUnrelated', kExprRefTest],
  ['refTestUnrelatedNull', kExprRefTestNull],
]) {
  builder.addFunction(name, makeSig([kWasmExternRef], [kWasmI32])).addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprAnyConvertExtern,
    kExprLocalTee, 1,
    kGCPrefix, kExprRefCastNull, structB,
    kExprDrop,
    kExprLocalGet, 1,
    kGCPrefix, opcode, structA,
  ]).addLocals(kWasmAnyRef, 1).exportFunc();
}

// Keep the tested value in an anyref local so the decoder does not fold the
// ref.test before the GC type analyzer can refine its input type.
for (const [name, opcode] of [
  ['refTestSubtype', kExprRefCast],
  ['refTestNullableSubtype', kExprRefCastNull],
]) {
  builder.addFunction(name, kSig_i_r).addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprAnyConvertExtern,
    kExprLocalTee, 1,
    kGCPrefix, opcode, structB,
    kExprDrop,
    kExprLocalGet, 1,
    kGCPrefix, kExprRefTest, structB,
  ]).addLocals(kWasmAnyRef, 1).exportFunc();
}

const baseStruct = builder.addStruct({fields: [], final: false});
const derivedStruct =
    builder.addStruct({fields: [], supertype: baseStruct, final: true});
builder.addFunction('refTestRefinedSource', kSig_i_r).addBody([
  kExprLocalGet, 0,
  kGCPrefix, kExprAnyConvertExtern,
  kExprLocalTee, 1,
  kGCPrefix, kExprRefCastNull, baseStruct,
  kExprDrop,
  kExprLocalGet, 1,
  kGCPrefix, kExprRefTest, derivedStruct,
]).addLocals(kWasmAnyRef, 1).exportFunc();

for (const [name, type] of [
  ['newBaseStructAsExternref', baseStruct],
  ['newDerivedStructAsExternref', derivedStruct],
]) {
  builder.addFunction(name, kSig_r_v).addBody([
    kGCPrefix, kExprStructNewDefault, type,
    kGCPrefix, kExprExternConvertAny,
  ]).exportFunc();
}

builder.addFunction('refI31Type', kSig_i_i).addBody([
  kExprLocalGet, 0,
  kGCPrefix, kExprRefI31,
  kExprLocalSet, 1,
  kExprLocalGet, 1,
  kGCPrefix, kExprRefTest, kI31RefCode,
]).addLocals(kWasmAnyRef, 1).exportFunc();

builder.addFunction('typeBasedAliasing', makeSig([kWasmExternRef, kWasmExternRef], [kWasmI32])).addBody([
  kExprLocalGet, 0,
  kGCPrefix, kExprAnyConvertExtern,
  kGCPrefix, kExprRefCast, structA,
  kExprLocalSet, 2,
  kExprLocalGet, 1,
  kGCPrefix, kExprAnyConvertExtern,
  kGCPrefix, kExprRefCast, structB,
  kExprLocalSet, 3,
  kExprLocalGet, 2,
  kGCPrefix, kExprStructGet, structA, 0,
  kExprDrop,
  kExprLocalGet, 3,
  kExprF32Const, 0, 0, 0, 0,
  kGCPrefix, kExprStructSet, structB, 0,
  kExprLocalGet, 2,
  kGCPrefix, kExprStructGet, structA, 0,
]).addLocals(wasmRefNullType(structA), 1).addLocals(wasmRefNullType(structB), 1).exportFunc();

// Helper to allocate structA as externref
builder.addFunction('newStructAAsExternref', makeSig([kWasmI32], [kWasmExternRef])).addBody([
  kExprLocalGet, 0,
  kGCPrefix, kExprStructNew, structA,
  kGCPrefix, kExprExternConvertAny,
]).exportFunc();

// Helper to allocate structB as externref
builder.addFunction('newStructBAsExternref', makeSig([kWasmF32], [kWasmExternRef])).addBody([
  kExprLocalGet, 0,
  kGCPrefix, kExprStructNew, structB,
  kGCPrefix, kExprExternConvertAny,
]).exportFunc();

// Helper to get array as externref
builder.addFunction('getArrayAsExternref', makeSig([], [kWasmExternRef])).addBody([
  kExprGlobalGet, globalArray.index,
  kGCPrefix, kExprExternConvertAny,
]).exportFunc();

// =============================================================================
// Instantiation & Global Variable Setup
// =============================================================================

const instance = builder.instantiate({});
const wasmExports = instance.exports;
wasmExports.initGlobalArray();

const arrayObj = wasmExports.getArrayAsExternref();
const structObj = wasmExports.newStructAsExternref(42);
const structAObj = wasmExports.newStructAAsExternref(42);
const structBObj = wasmExports.newStructBAsExternref(3.14);
const baseStructObj = wasmExports.newBaseStructAsExternref();
const derivedStructObj = wasmExports.newDerivedStructAsExternref();

// =============================================================================
// JS Execution & Optimization Tests
// =============================================================================

// WasmGCTypedOptimizationReducer tests

// CHECK-LABEL: Considering Wasm function [{{[0-9]+}}] redundantRefCast of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
// During WasmInJSInliningPhase:
// - The RTT Map is baked in as a HeapConstant (rather than loaded at runtime via RttCanon).
// - The second identical ref.cast (non-null) is GVN-deduplicated on-the-fly during inlining,
//   so the next operation emitted is immediately the third cast (ref.cast null), reusing the
//   same GVN'd RTT HeapConstant [[RTT_CONST]].
// CHECK: n[[ANY_CONV:[0-9]+]]:    AnyConvertExtern
// CHECK-NEXT: n[[RTT_CONST:[0-9]+]]:    Constant()[heap object: {{.*}} <Map{{.*}}>]
// CHECK-NEXT: n{{[0-9]+}}:    WasmTypeCast(#n[[ANY_CONV]], #n[[RTT_CONST]], #n{{[0-9]+}})[anyref -> (ref 0) @kMayBeSubtype]
// CHECK-NEXT: n{{[0-9]+}}:    WasmTypeCast(#n[[ANY_CONV]], #n[[RTT_CONST]], #n{{[0-9]+}})[anyref -> (ref null 0) @kMayBeSubtype]
//
// During WasmGCOptimizePhase:
// - WasmGCTypeAnalyzer refines the input type to (ref 0).
// - WasmGCTypedOptimizationReducer keeps the first WasmTypeCast and eliminates the third
//   (nullable) WasmTypeCast.
// CHECK: WasmTypeCast{{.*}} Refine type for object #[[ANY_CONV]](AnyConvertExtern) -> (ref 0)
// CHECK: o{{[0-9]+}}:    AnyConvertExtern
// CHECK-NEXT: ╰─> n[[OPT_ANY:[0-9]+]]:    AnyConvertExtern
// CHECK: o{{[0-9]+}}:    WasmTypeCast{{.*}}[anyref -> (ref 0) @kMayBeSubtype]
// CHECK-NEXT: ╰─> n{{[0-9]+}}:    WasmTypeCast{{.*}}[anyref -> (ref 0) @kMayBeSubtype]
// CHECK: o{{[0-9]+}}:    WasmTypeCast{{.*}}[anyref -> (ref null 0) @kMayBeSubtype]
// CHECK-NEXT: ╰─> #n[[OPT_ANY]]
function js_redundantRefCast(arg) {
  return wasmExports.redundantRefCast(arg);
}
testOptimized(js_redundantRefCast, [42, arrayObj]);

// CHECK-LABEL: Considering Wasm function [{{[0-9]+}}] redundantConversion of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
// CHECK: o{{[0-9]+}}:    AnyConvertExtern
// CHECK-NEXT: ╰─> n{{[0-9]+}}:    AnyConvertExtern
// CHECK: o{{[0-9]+}}:    ExternConvertAny
// CHECK-NEXT: ╰─> n{{[0-9]+}}:    ExternConvertAny
// CHECK: o{{[0-9]+}}:    AnyConvertExtern
// CHECK-NEXT: ╰─> #n{{[0-9]+}}
function js_redundantConversion(arg) {
  return wasmExports.redundantConversion(arg);
}
testOptimized(js_redundantConversion, [42, arrayObj]);

// CHECK-LABEL: Considering Wasm function [{{[0-9]+}}] redundantIsNull of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
// CHECK: o{{[0-9]+}}:    IsNull
// CHECK-NEXT: ╰─> n{{[0-9]+}}:    Constant
function js_redundantIsNull(arg) {
  return wasmExports.redundantIsNull(arg);
}
testOptimized(js_redundantIsNull, [0, arrayObj]);

// CHECK-LABEL: Considering Wasm function [{{[0-9]+}}] constantFunctionRefIsNull of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
// CHECK: n{{[0-9]+}}:    WasmRefFunc
// CHECK: n{{[0-9]+}}:    Constant
function js_constantFunctionRefIsNull() {
  return wasmExports.constantFunctionRefIsNull();
}
testOptimized(js_constantFunctionRefIsNull, [0]);

// CHECK-LABEL: Considering Wasm function [{{[0-9]+}}] refTestUnrelated of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
// CHECK: o{{[0-9]+}}:    WasmTypeCheck
// CHECK-NEXT: ╰─> n{{[0-9]+}}:    Constant()[word32: 0]
function js_refTestUnrelated(arg) {
  return wasmExports.refTestUnrelated(arg);
}
testOptimized(js_refTestUnrelated, [0, structBObj], [0, null]);
assertTraps(kTrapIllegalCast, () => js_refTestUnrelated(structAObj));

// CHECK-LABEL: Considering Wasm function [{{[0-9]+}}] refTestUnrelatedNull of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
// CHECK: o{{[0-9]+}}:    WasmTypeCheck
// CHECK-NEXT: ╰─> n{{[0-9]+}}:    IsNull
function js_refTestUnrelatedNull(arg) {
  return wasmExports.refTestUnrelatedNull(arg);
}
testOptimized(js_refTestUnrelatedNull, [0, structBObj], [1, null]);
assertTraps(kTrapIllegalCast, () => js_refTestUnrelatedNull(structAObj));

// CHECK-LABEL: Considering Wasm function [{{[0-9]+}}] refTestSubtype of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
// CHECK: o{{[0-9]+}}:    WasmTypeCheck
// CHECK-NEXT: ╰─> n{{[0-9]+}}:    Constant()[word32: 1]
function js_refTestSubtype(arg) {
  return wasmExports.refTestSubtype(arg);
}
testOptimized(js_refTestSubtype, [1, structBObj]);
assertTraps(kTrapIllegalCast, () => js_refTestSubtype(null));
assertTraps(kTrapIllegalCast, () => js_refTestSubtype(structAObj));

// CHECK-LABEL: Considering Wasm function [{{[0-9]+}}] refTestNullableSubtype of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
// CHECK: o{{[0-9]+}}:    WasmTypeCheck
// CHECK-NEXT: {{.*}} n[[SUBTYPE_NULL:[0-9]+]]:    IsNull
// CHECK-NEXT: {{.*}} n[[SUBTYPE_ZERO:[0-9]+]]:    Constant()[word32: 0]
// CHECK-NEXT: ╰─> n{{[0-9]+}}:    Comparison(#n[[SUBTYPE_NULL]], #n[[SUBTYPE_ZERO]])[Equal, Word32]
function js_refTestNullableSubtype(arg) {
  return wasmExports.refTestNullableSubtype(arg);
}
testOptimized(js_refTestNullableSubtype, [1, structBObj], [0, null]);
assertTraps(kTrapIllegalCast, () => js_refTestNullableSubtype(structAObj));

// CHECK-LABEL: Considering Wasm function [{{[0-9]+}}] refTestRefinedSource of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
// CHECK: o{{[0-9]+}}:    WasmTypeCast{{.*}}[anyref -> (ref null [[BASE:[0-9]+]]) @kMayBeSubtype]
// CHECK: o{{[0-9]+}}:    WasmTypeCheck{{.*}}[anyref -> (ref [[DERIVED:[0-9]+]]) @kExactMatchOnly]
// CHECK-NEXT: ╰─> n{{[0-9]+}}:    WasmTypeCheck{{.*}}[(ref null [[BASE]]) -> (ref [[DERIVED]]) @kExactMatchOnly]
function js_refTestRefinedSource(arg) {
  return wasmExports.refTestRefinedSource(arg);
}
testOptimized(
    js_refTestRefinedSource, [0, baseStructObj], [1, derivedStructObj], [0, null]);
assertTraps(kTrapIllegalCast, () => js_refTestRefinedSource(structBObj));

// CHECK-LABEL: Considering Wasm function [{{[0-9]+}}] refI31Type of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
// CHECK: n[[I31:[0-9]+]]:    WasmRefI31{{.*}}[non-shared]
// CHECK: o{{[0-9]+}}:    WasmTypeCheck(#o[[I31]])[anyref -> (ref i31)
// CHECK-NEXT: ╰─> n{{[0-9]+}}:    Constant()[word32: 1]
function js_refI31Type(arg) {
  return wasmExports.refI31Type(arg);
}
testOptimized(
    js_refI31Type, [1, 0], [1, -1], [1, 0x3fffffff], [1, 0x40000000],
    [1, -0x40000000], [1, -0x40000001]);


// WasmLoadEliminationReducer tests

// CHECK-LABEL: Considering Wasm function [{{[0-9]+}}] redundantStructGet of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
// 1. The first StructGet is copied.
// CHECK: o{{[0-9]+}}:    StructGet
// CHECK-NEXT: ╰─> n{{[0-9]+}}:    StructGet
//
// NOTE: The second StructGet in redundantStructGet's Wasm body is already GVN'd and eliminated
// on-the-fly during WasmInJSInliningPhase, so it is completely absent in this phase!
function js_redundantStructGet(arg) {
  return wasmExports.redundantStructGet(arg);
}
testOptimized(js_redundantStructGet, [84, structObj]);


// CHECK-LABEL: Considering Wasm function [{{[0-9]+}}] storeToLoadForwarding of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
// CHECK: o{{[0-9]+}}:    StructSet
// CHECK-NEXT: {{.*}} n{{[0-9]+}}:    StructSet
// CHECK: o{{[0-9]+}}:    StructGet
// CHECK-NEXT: ╰─> #n{{[0-9]+}}
function js_storeToLoadForwarding(arg0, arg1) {
  return wasmExports.storeToLoadForwarding(arg0, arg1);
}
testOptimized(js_storeToLoadForwarding, [100, structObj, 100]);

// CHECK-LABEL: Considering Wasm function [{{[0-9]+}}] aliasingBoundsOnStore of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
// CHECK: o{{[0-9]+}}:    StructGet
// CHECK-NEXT: ╰─> n{{[0-9]+}}:    StructGet
// CHECK: o{{[0-9]+}}:    StructSet
// CHECK-NEXT: {{.*}} n{{[0-9]+}}:    StructSet
// CHECK: o{{[0-9]+}}:    StructGet
// CHECK-NEXT: ╰─> #n{{[0-9]+}}
// CHECK: o{{[0-9]+}}:    StructSet
// CHECK-NEXT: {{.*}} n{{[0-9]+}}:    StructSet
// CHECK: o{{[0-9]+}}:    StructGet
// CHECK-NEXT: ╰─> n{{[0-9]+}}:    StructGet
function js_aliasingBoundsOnStore(arg0, arg1) {
  return wasmExports.aliasingBoundsOnStore(arg0, arg1);
}
testOptimized(js_aliasingBoundsOnStore, [42, structObj, structObj]);

// CHECK-LABEL: Considering Wasm function [{{[0-9]+}}] typeBasedAliasing of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
// CHECK: o{{[0-9]+}}:    StructGet
// CHECK-NEXT: ╰─> n{{[0-9]+}}:    StructGet
// CHECK: o{{[0-9]+}}:    StructSet
// CHECK-NEXT: {{.*}} n{{[0-9]+}}:    StructSet
// CHECK: o{{[0-9]+}}:    StructGet
// CHECK-NEXT: ╰─> #n{{[0-9]+}}
function js_typeBasedAliasing(arg0, arg1) {
  return wasmExports.typeBasedAliasing(arg0, arg1);
}
testOptimized(js_typeBasedAliasing, [42, structAObj, structBObj]);

// CHECK-LABEL: Considering Wasm function [{{[0-9]+}}] structGetOnce of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
// CHECK: n[[STRUCT_ANY:[0-9]+]]:    AnyConvertExtern
// CHECK-NEXT: n[[STRUCT_RTT:[0-9]+]]:    Constant()[heap object: {{.*}} <Map{{.*}}>]
// CHECK-NEXT: n{{[0-9]+}}:    WasmTypeCast(#n[[STRUCT_ANY]], #n[[STRUCT_RTT]], #n{{[0-9]+}})
//
// The Wasm callee structGetOnce is inlined twice (once for each JS call in the expression).
// Because the RTT Map is baked in as a HeapConstant, the second inlined call reuses #n[[STRUCT_RTT]]
// via GVN in WasmInJSInliningPhase (while the two WasmTypeCast operations remain distinct until
// WasmGCOptimizePhase because their FrameState inputs differ).
// CHECK: Considering Wasm function [{{[0-9]+}}] structGetOnce of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
// CHECK: n{{[0-9]+}}:    WasmTypeCast(#n[[STRUCT_ANY]], #n[[STRUCT_RTT]], #n{{[0-9]+}})
//
// Assert that the WasmGCTypeAnalyzer successfully refines the type of the unified AnyConvertExtern
// node to the expected struct type (ref index).
// CHECK: Refine type for object #{{[0-9]+}}(AnyConvertExtern) -> (ref [[RTT_INDEX:[0-9]+]])
// CHECK: Refine type for object #{{[0-9]+}}(StructGet) -> i32
//
// Then assert that the graph reduction / copying output shows optimizations for
// the second inlined call.
// First inlined Wasm body:
// CHECK: o{{[0-9]+}}:    AnyConvertExtern
// CHECK-NEXT: ╰─> n[[EXTERN_ID:[0-9]+]]:    AnyConvertExtern
// CHECK: o{{[0-9]+}}:    WasmTypeCast
// CHECK-NEXT: ╰─> n{{[0-9]+}}:    WasmTypeCast
// CHECK: o{{[0-9]+}}:    StructGet
// CHECK-NEXT: ╰─> n[[LOAD_ID:[0-9]+]]:    StructGet
//
// Second inlined Wasm body:
// * GVN in WasmInJSInliningPhase already unified the AnyConvertExtern node.
// * WasmGCTypedOptimizationReducer performs Type Cast Elimination on the second WasmTypeCast (ref.cast).
//   Since the unified input node [[EXTERN_ID]] already has a refined type of (ref [[RTT_INDEX]]) from the first cast,
//   this second cast is redundant. It is bypassed and returns the raw unified input node [[EXTERN_ID]].
// CHECK: o{{[0-9]+}}:    WasmTypeCast
// CHECK-NEXT: ╰─> #n[[EXTERN_ID]]
// * WasmLoadElimination successfully eliminates the second StructGet (struct.get) load because
//   it now resolves to the identical base object [[EXTERN_ID]].
//   It is replaced by the result of the first load [[LOAD_ID]].
// CHECK: o{{[0-9]+}}:    StructGet
// CHECK-NEXT: ╰─> #n[[LOAD_ID]]
function js_crossCallLoadElimination(arg) {
  return wasmExports.structGetOnce(arg) + wasmExports.structGetOnce(arg);
}
testOptimized(js_crossCallLoadElimination, [84, structObj]);

// =============================================================================
// Unsupported tests due to limitations in Wasm-in-JS inlining:
// =============================================================================
// 1. Allocations (struct.new / array.new) are unsupported inside inlinees:
//    - gc-optimizations.js: LoadEliminationFreshKnownTest
//    - gc-optimizations.js: LoadEliminationArbitraryKnownTest
//    - gc-optimizations.js: LoadEliminationFreshUnknownTest
//    - gc-optimizations.js: LoadEliminationAllBetsAreOffTest
//    - gc-optimizations.js: WasmLoadEliminationArrayLength
//    - gc-optimizations.js: EscapeAnalysisWithLoadElimination
//    - gc-optimizations.js: EscapeAnalysisWithInterveningEffect
//    - gc-optimizations.js: AllocationFolding
//    - gc-optimizations.js: ArrayNewRefTest
//
// 2. Control flow (loops / branches) are unsupported inside inlinees:
//    - gc-optimizations.js: StructGetMultipleNullChecks
//    - gc-optimizations.js: StructSetMultipleNullChecks
//    - gc-optimizations.js: ArrayLenMultipleNullChecks
//    - gc-optimizations.js: TypePropagationPhi
//    - gc-optimizations.js: TypePropagationLoopPhiOptimizable
//    - gc-optimizations.js: TypePropagationLoopPhiCheckRequired
//    - gc-optimizations.js: TypePropagationLoopPhiCheckRequiredUnrelated
//    - gc-optimizations.js: TypePropagationCallRef
//    - gc-optimizations.js: TypePropagationDeadBranch
//    - gc-optimizations.js: TypePropagationDeadByRefTestTrue
//    - gc-optimizations.js: TypePropagationDeadByRefTestFalse
//    - gc-optimizations.js: TypePropagationDeadByIsNull
//    - type-based-optimizations.js: WasmTypedOptimizationsTest (uses loops/branches)
