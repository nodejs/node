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
  kExprLocalGet, 1,
  kGCPrefix, kExprRefCast, array,
  kExprDrop,
  kExprLocalGet, 1,
  kGCPrefix, kExprRefCast, array,
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

// =============================================================================
// JS Execution & Optimization Tests
// =============================================================================

// WasmGCTypedOptimizationReducer tests

// CHECK-LABEL: Considering Wasm function [{{[0-9]+}}] redundantRefCast of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
// CHECK: WasmTypeCast{{.*}} Refine type for object {{.*}} -> (ref 0)
// CHECK: o{{[0-9]+}}:    WasmTypeCast
// CHECK-NEXT: ╰─> n{{[0-9]+}}:    WasmTypeCast
// CHECK: o{{[0-9]+}}:    WasmTypeCast
// CHECK-NEXT: ╰─> #n{{[0-9]+}}
function js_redundantRefCast(arg) {
  return wasmExports.redundantRefCast(arg);
}
%PrepareFunctionForOptimization(js_redundantRefCast);
for (let i = 0; i < 10; ++i) assertEquals(42, js_redundantRefCast(arrayObj));
%OptimizeFunctionOnNextCall(js_redundantRefCast);
assertEquals(42, js_redundantRefCast(arrayObj));

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
%PrepareFunctionForOptimization(js_redundantConversion);
for (let i = 0; i < 10; ++i) assertEquals(42, js_redundantConversion(arrayObj));
%OptimizeFunctionOnNextCall(js_redundantConversion);
assertEquals(42, js_redundantConversion(arrayObj));

// CHECK-LABEL: Considering Wasm function [{{[0-9]+}}] redundantIsNull of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
// CHECK: o{{[0-9]+}}:    IsNull
// CHECK-NEXT: ╰─> n{{[0-9]+}}:    Constant
function js_redundantIsNull(arg) {
  return wasmExports.redundantIsNull(arg);
}
%PrepareFunctionForOptimization(js_redundantIsNull);
for (let i = 0; i < 10; ++i) assertEquals(0, js_redundantIsNull(arrayObj));
%OptimizeFunctionOnNextCall(js_redundantIsNull);
assertEquals(0, js_redundantIsNull(arrayObj));

// CHECK-LABEL: Considering Wasm function [{{[0-9]+}}] constantFunctionRefIsNull of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
// CHECK: n{{[0-9]+}}:    WasmRefFunc
// CHECK: n{{[0-9]+}}:    Constant
function js_constantFunctionRefIsNull() {
  return wasmExports.constantFunctionRefIsNull();
}
%PrepareFunctionForOptimization(js_constantFunctionRefIsNull);
for (let i = 0; i < 10; ++i) assertEquals(0, js_constantFunctionRefIsNull());
%OptimizeFunctionOnNextCall(js_constantFunctionRefIsNull);
assertEquals(0, js_constantFunctionRefIsNull());


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
%PrepareFunctionForOptimization(js_redundantStructGet);
for (let i = 0; i < 10; ++i) assertEquals(84, js_redundantStructGet(structObj));
%OptimizeFunctionOnNextCall(js_redundantStructGet);
assertEquals(84, js_redundantStructGet(structObj));


// CHECK-LABEL: Considering Wasm function [{{[0-9]+}}] storeToLoadForwarding of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
// CHECK: o{{[0-9]+}}:    StructSet
// CHECK-NEXT: {{.*}} n{{[0-9]+}}:    StructSet
// CHECK: o{{[0-9]+}}:    StructGet
// CHECK-NEXT: ╰─> #n{{[0-9]+}}
function js_storeToLoadForwarding(arg0, arg1) {
  return wasmExports.storeToLoadForwarding(arg0, arg1);
}
%PrepareFunctionForOptimization(js_storeToLoadForwarding);
for (let i = 0; i < 10; ++i) assertEquals(100, js_storeToLoadForwarding(structObj, 100));
%OptimizeFunctionOnNextCall(js_storeToLoadForwarding);
assertEquals(100, js_storeToLoadForwarding(structObj, 100));

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
%PrepareFunctionForOptimization(js_aliasingBoundsOnStore);
for (let i = 0; i < 10; ++i) assertEquals(42, js_aliasingBoundsOnStore(structObj, structObj));
%OptimizeFunctionOnNextCall(js_aliasingBoundsOnStore);
assertEquals(42, js_aliasingBoundsOnStore(structObj, structObj));

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
%PrepareFunctionForOptimization(js_typeBasedAliasing);
for (let i = 0; i < 10; ++i) assertEquals(42, js_typeBasedAliasing(structAObj, structBObj));
%OptimizeFunctionOnNextCall(js_typeBasedAliasing);
assertEquals(42, js_typeBasedAliasing(structAObj, structBObj));

// CHECK-LABEL: Considering Wasm function [{{[0-9]+}}] structGetOnce of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
//
// The Wasm callee structGetOnce is inlined twice (once for each JS call in the expression).
// CHECK: Considering Wasm function [{{[0-9]+}}] structGetOnce of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
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
%PrepareFunctionForOptimization(js_crossCallLoadElimination);
for (let i = 0; i < 10; ++i) assertEquals(84, js_crossCallLoadElimination(structObj));
%OptimizeFunctionOnNextCall(js_crossCallLoadElimination);
assertEquals(84, js_crossCallLoadElimination(structObj));

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
//
// 3. Explicit type check bytecodes (ref.test) are unsupported inside inlinees:
//    - gc-optimizations.js: RefTestUnrelated
//    - gc-optimizations.js: ArrayNewRefTest
