// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbolev --turbolev-inline-js-wasm-wrappers
// Flags: --turboshaft-wasm-in-js-inlining
// Flags: --allow-natives-syntax
// Flags: --no-always-sparkplug
// Only tier-up the test functions to get a cleaner and stable trace.
// Flags: --trace-turbo-inlining --turbo-filter='js_*'
// Concurrent inlining leads to additional traces.
// Flags: --no-stress-concurrent-inlining

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");
d8.file.execute("test/mjsunit/mjsunit.js");

const builder = new WasmModuleBuilder();
const array = builder.addArray(kWasmI32);
const globalI32 = builder.addGlobal(kWasmI32, true, false);
const globalEqRef = builder.addGlobal(kWasmEqRef, true, false);

let tests = [];
// A helper to avoid repetition (e.g., the Wasm function name) and keep all
// pieces of one test case in one place (e.g., JS arguments, Wasm signature).
function addTestcase(name, wasmSignature, wasmArguments, wasmBody, jsFunctionBuilder) {
  assertEquals(wasmSignature.params.length, wasmArguments.length);
  const wasmFunction = builder.addFunction(name, wasmSignature).addBody(wasmBody).exportFunc();

  if (!jsFunctionBuilder) {
    // Pass the arguments from the JS function to the Wasm function so that they
    // are not constant after inlining (otherwise constant folding often removes
    // the code from the inlinee).
    // Also, do not use the spread syntax (...args) to call the Wasm export, but
    // instead explicitly list the arguments. Otherwise the JS-to-Wasm wrapper
    // will not be inlined.
    // TODO(353475584): Is it worth lifting this restriction? Is this a
    // restriction also for regular JS inlining?
    const argumentList = Array.from(wasmArguments, (x, i) => 'arg' + i);
    jsFunctionBuilder = eval(`
      (wasmExports) => {
        return function js_${name}(${argumentList}) {
            return wasmExports.${name}(${argumentList});
        };
      }
    `);
  }
  tests[name] = { wasmArguments, jsFunctionBuilder };

    // Return the wasmFunction, so that it can still be manually modified.
  return wasmFunction;
}

// =============================================================================
// Testcases where both JS-to-Wasm wrapper inlining and Wasm body inlining
// succeed:
// =============================================================================

// CHECK: Considering wasm function [{{[0-9]+}}] empty of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addTestcase('empty', kSig_v_v, [], []);
// CHECK: Considering wasm function [{{[0-9]+}}] nop of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addTestcase('nop', kSig_v_v, [], [kExprNop]);

// CHECK: Considering wasm function [{{[0-9]+}}] i32Const of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addTestcase('i32Const', kSig_i_v, [], [...wasmI32Const(42)]);
// CHECK: Considering wasm function [{{[0-9]+}}] f32Const of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addTestcase('f32Const', kSig_f_v, [], [...wasmF32Const(42.0)]);
// CHECK: Considering wasm function [{{[0-9]+}}] f64Const of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addTestcase('f64Const', kSig_d_v, [], [...wasmF64Const(42.0)]);

function addUnaryTestcase(op, wasmSignature, wasmArgument) {
  addTestcase(op, wasmSignature, [wasmArgument], [
    kExprLocalGet, 0,
    eval('kExpr' + op)
  ]);
}
// CHECK: Considering wasm function [{{[0-9]+}}] I32Eqz of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addUnaryTestcase('I32Eqz', kSig_i_i, 0);
// CHECK: Considering wasm function [{{[0-9]+}}] F32Abs of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addUnaryTestcase('F32Abs', kSig_f_f, 0);
// CHECK: Considering wasm function [{{[0-9]+}}] F32Neg of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addUnaryTestcase('F32Neg', kSig_f_f, 0);
// CHECK: Considering wasm function [{{[0-9]+}}] F32Sqrt of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addUnaryTestcase('F32Sqrt', kSig_f_f, 0);
// CHECK: Considering wasm function [{{[0-9]+}}] F64Abs of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addUnaryTestcase('F64Abs', kSig_d_d, 0);
// CHECK: Considering wasm function [{{[0-9]+}}] F64Neg of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addUnaryTestcase('F64Neg', kSig_d_d, 0);
// CHECK: Considering wasm function [{{[0-9]+}}] F64Sqrt of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addUnaryTestcase('F64Sqrt', kSig_d_d, 0);
// CHECK: Considering wasm function [{{[0-9]+}}] F64SConvertI32 of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addUnaryTestcase('F64SConvertI32', kSig_d_i, 0);
// CHECK: Considering wasm function [{{[0-9]+}}] F64UConvertI32 of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addUnaryTestcase('F64UConvertI32', kSig_d_i, 0);
// CHECK: Considering wasm function [{{[0-9]+}}] F32SConvertI32 of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addUnaryTestcase('F32SConvertI32', kSig_f_i, 0);
// CHECK: Considering wasm function [{{[0-9]+}}] F32UConvertI32 of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addUnaryTestcase('F32UConvertI32', kSig_f_i, 0);
// CHECK: Considering wasm function [{{[0-9]+}}] F32ConvertF64 of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addUnaryTestcase('F32ConvertF64', kSig_f_d, 0);
// CHECK: Considering wasm function [{{[0-9]+}}] F64ConvertF32 of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addUnaryTestcase('F64ConvertF32', kSig_d_f, 0);
// CHECK: Considering wasm function [{{[0-9]+}}] F32ReinterpretI32 of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addUnaryTestcase('F32ReinterpretI32', kSig_f_i, 0);
// CHECK: Considering wasm function [{{[0-9]+}}] I32ReinterpretF32 of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addUnaryTestcase('I32ReinterpretF32', kSig_i_f, 0);
// CHECK: Considering wasm function [{{[0-9]+}}] I32Clz of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addUnaryTestcase('I32Clz', kSig_i_i, 0);
// CHECK: Considering wasm function [{{[0-9]+}}] I32SExtendI8 of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addUnaryTestcase('I32SExtendI8', kSig_i_i, 0);
// CHECK: Considering wasm function [{{[0-9]+}}] I32SExtendI16 of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addUnaryTestcase('I32SExtendI16', kSig_i_i, 0);
// CHECK: Considering wasm function [{{[0-9]+}}] RefIsNull of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addUnaryTestcase('RefIsNull', kSig_i_r, null);
// We cannot pass Wasm any refs across the JS-Wasm boundary, hence convert to
// any and back in one test.
// TODO(dlehmann): Update output once arm64 no-ptr-compression calls are fixed.
// CHECK: Considering wasm function [{{[0-9]+}}] anyConvertExternConvertAny of module {{.*}} for inlining
// CHECK-NEXT: - not inlining: unsupported operation: any.convert_extern
addTestcase('anyConvertExternConvertAny', kSig_r_r, [{}], [
  kExprLocalGet, 0,
  kGCPrefix, kExprAnyConvertExtern,
  kGCPrefix, kExprExternConvertAny,
]);

function addBinaryTestcase(op, wasmSignature, wasmArgument0, wasmArgument1) {
  addTestcase(op, wasmSignature, [wasmArgument0, wasmArgument1], [
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    eval('kExpr' + op)
  ]);
}
// CHECK: Considering wasm function [{{[0-9]+}}] I32Add of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('I32Add', kSig_i_ii, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] I32Sub of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('I32Sub', kSig_i_ii, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] I32Mul of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('I32Mul', kSig_i_ii, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] I32And of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('I32And', kSig_i_ii, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] I32Ior of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('I32Ior', kSig_i_ii, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] I32Xor of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('I32Xor', kSig_i_ii, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] I32Shl of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('I32Shl', kSig_i_ii, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] I32ShrS of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('I32ShrS', kSig_i_ii, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] I32ShrU of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('I32ShrU', kSig_i_ii, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] I32Ror of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('I32Ror', kSig_i_ii, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] I32Rol of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('I32Rol', kSig_i_ii, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] I32Eq of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('I32Eq', kSig_i_ii, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] I32Ne of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('I32Ne', kSig_i_ii, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] I32LtS of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('I32LtS', kSig_i_ii, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] I32LeS of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('I32LeS', kSig_i_ii, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] I32LtU of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('I32LtU', kSig_i_ii, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] I32LeU of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('I32LeU', kSig_i_ii, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] I32GtS of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('I32GtS', kSig_i_ii, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] I32GeS of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('I32GeS', kSig_i_ii, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] I32GtU of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('I32GtU', kSig_i_ii, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] I32GeU of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('I32GeU', kSig_i_ii, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] F32CopySign of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('F32CopySign', kSig_f_ff, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] F32Add of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('F32Add', kSig_f_ff, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] F32Sub of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('F32Sub', kSig_f_ff, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] F32Mul of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('F32Mul', kSig_f_ff, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] F32Div of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('F32Div', kSig_f_ff, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] F32Eq of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('F32Eq', kSig_i_ff, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] F32Ne of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('F32Ne', kSig_i_ff, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] F32Lt of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('F32Lt', kSig_i_ff, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] F32Le of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('F32Le', kSig_i_ff, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] F32Gt of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('F32Gt', kSig_i_ff, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] F32Ge of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('F32Ge', kSig_i_ff, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] F32Min of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('F32Min', kSig_f_ff, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] F32Max of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('F32Max', kSig_f_ff, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] F64Add of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('F64Add', kSig_d_dd, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] F64Sub of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('F64Sub', kSig_d_dd, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] F64Mul of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('F64Mul', kSig_d_dd, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] F64Div of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('F64Div', kSig_d_dd, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] F64Eq of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('F64Eq', kSig_i_dd, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] F64Ne of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('F64Ne', kSig_i_dd, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] F64Lt of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('F64Lt', kSig_i_dd, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] F64Le of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('F64Le', kSig_i_dd, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] F64Gt of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('F64Gt', kSig_i_dd, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] F64Ge of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('F64Ge', kSig_i_dd, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] F64Min of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('F64Min', kSig_d_dd, 3, 7);
// CHECK: Considering wasm function [{{[0-9]+}}] F64Max of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addBinaryTestcase('F64Max', kSig_d_dd, 3, 7);
// This currently cannot be inlined when passing `eqref`s as an argument from JS
// because the JS-to-Wasm wrapper inlining bails out in that case.
// So go through through globals instead.
// CHECK: Considering wasm function [{{[0-9]+}}] RefEq of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addTestcase('RefEq', kSig_i_v, [], [
  kExprGlobalGet, globalEqRef.index,
  kExprGlobalGet, globalEqRef.index,
  kExprRefEq,
])

// Function arguments and locals.
// CHECK: Considering wasm function [{{[0-9]+}}] passthroughI32 of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addTestcase('passthroughI32', kSig_i_i, [13], [kExprLocalGet, 0]);
// CHECK: Considering wasm function [{{[0-9]+}}] localTee of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addTestcase('localTee', kSig_i_i, [42], [
  kExprLocalGet, 0,
  ...wasmI32Const(7),
  kExprLocalTee, 1,
  kExprI32Add,
  kExprLocalGet, 1,
  kExprI32Add,
]).addLocals(kWasmI32, 1);
// CHECK: Considering wasm function [{{[0-9]+}}] localSwap of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addTestcase('localSwap', kSig_i_i, [42], [
  ...wasmI32Const(3),
  kExprLocalSet, 2,  // Initialize local_2 = 3.

  // Ring-swap: local 2 to parameter, local 1 to local 2, parameter to local 1,
  // stack as temporary.
  kExprLocalGet, 0,
  kExprLocalGet, 2,
  kExprLocalSet, 0,
  kExprLocalGet, 1,
  kExprLocalSet, 2,
  kExprLocalSet, 1,

  // Use subtraction since it's not commutative.
  kExprLocalGet, 2,  // value: 0
  kExprLocalGet, 1,  // value: parameter
  kExprI32Sub,
  kExprLocalGet, 0,  // value: 3
  kExprI32Sub,  // = (0 - p) - 3 = -10
]).addLocals(kWasmI32, 2);

// CHECK: Considering wasm function [{{[0-9]+}}] globalSetGet of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addTestcase('globalSetGet', kSig_i_i, [42], [
  kExprLocalGet, 0,
  kExprGlobalSet, globalI32.index,
  kExprGlobalGet, globalI32.index,
  kExprGlobalGet, globalI32.index,
  kExprI32Add,
  kExprGlobalGet, globalI32.index,
  kExprI32Add,
]);

// CHECK: Inlining JS-to-Wasm wrapper for Wasm function [{{[0-9]+}}] unreachable of module {{.*}}
// CHECK-NEXT: Considering wasm function [{{[0-9]+}}] unreachable of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addTestcase('unreachable', kSig_v_v, [], [
  kExprUnreachable,
]);

const globalArray = builder.addGlobal(wasmRefNullType(array), true, false);

builder.addFunction('initGlobalArray', makeSig([], [])).addBody([
  ...wasmI32Const(42),
  kGCPrefix, kExprArrayNewDefault, array,
  kExprGlobalSet, globalArray.index,
]).exportFunc();

// CHECK: Considering wasm function [{{[0-9]+}}] arrayLenGlobal of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addTestcase('arrayLenGlobal', makeSig([], [kWasmI32]), [], [
  kExprGlobalGet, globalArray.index,
  kGCPrefix, kExprArrayLen,
], (wasmExports) => {
  wasmExports.initGlobalArray();
  return function js_arrayLenGlobal() {
    const len = wasmExports.arrayLenGlobal();
    return len;
  };
});

const globalNullArray = builder.addGlobal(wasmRefNullType(array), true, false);

// This will throw and should have the right error message and stack trace.
// CHECK: Considering wasm function [{{[0-9]+}}] arrayLenNull of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addTestcase('arrayLenNull', kSig_i_v, [], [
  kExprGlobalGet, globalNullArray.index,
  kGCPrefix, kExprArrayLen,
]);


// =============================================================================
// Testcases that we (currently) do not inline (the JS-to-Wasm wrapper or body):
// =============================================================================

// TODO(dlehmann,353475584): `createArray` requires allocation, which might get
// complicated with allocation folding, so that's not in the MVP yet.
// CHECK: Considering wasm function [{{[0-9]+}}] createArray of module {{.*}} for inlining
// CHECK-NEXT: - not inlining: unsupported operation: array.new_default
addTestcase('createArray', makeSig([kWasmI32], [kWasmExternRef]), [42], [
  kExprLocalGet, 0,
  kGCPrefix, kExprArrayNewDefault, array,
  kGCPrefix, kExprExternConvertAny,
]);
// CHECK: Considering wasm function [{{[0-9]+}}] arrayLenParam of module {{.*}} for inlining
// CHECK-NEXT: - not inlining: unsupported operation: any.convert_extern
addTestcase('arrayLenParam', makeSig([kWasmExternRef], [kWasmI32]), [null], [
  kExprLocalGet, 0,
  kGCPrefix, kExprAnyConvertExtern,
  kGCPrefix, kExprRefCastNull, array,
  kGCPrefix, kExprArrayLen,
], (wasmExports) => {
  const array = wasmExports.createArray(42);
  return function js_arrayLenParam() {
    wasmExports.arrayLenParam(array);
  };
});

// TODO(dlehmann,353475584): Support i64 in Wasm signatures when inlining the
// JS-to-Wasm wrapper on 32-bit architectures.
// addTestcase('passthroughI64', kSig_l_l, [13n], [kExprLocalGet, 0]);

// TODO(dlehmann,353475584): We don't support i64 ops yet, since that requires
// `Int64LoweringReducer`, which isn't compatible with the JS pipeline yet.
// Also, there is no wrapper inlining for i64 on 32-bit architectures right
// now, so pass as i32 on the boundary.
// CHECK: Considering wasm function [{{[0-9]+}}] i64Add of module {{.*}} for inlining
// CHECK-NEXT: - not inlining: unsupported operation: i64.extend_i32_s
addTestcase(`i64Add`, kSig_i_ii, [13, 23], [
  kExprLocalGet, 0,
  kExprI64SConvertI32,
  kExprLocalGet, 1,
  kExprI64SConvertI32,
  kExprI64Add,
  kExprI32ConvertI64,
]);

// TODO(dlehmann,353475584): This would require supporting multi-value in
// JS-to-Wasm wrapper inlining first. (Right now, we bailout already at this
// first step.)
// CHECK: Test: multiValue
// CHECK-NOT: Inlining JS-to-Wasm wrapper
// CHECK-NOT: - inlining
// CHECK: Result:
addTestcase('multiValue', kSig_ii_v, [], [
  ...wasmI32Const(3),
  ...wasmI32Const(7),
]);

// TODO(dlehmann,353475584): Support control-flow in the inlinee (much later).
// CHECK: Considering wasm function [{{[0-9]+}}] brNoInline of module {{.*}} for inlining
// CHECK-NEXT: - not inlining: unsupported operation: br
addTestcase('brNoInline', kSig_i_v, [], [
  ...wasmI32Const(42),
  // Need to use a `br`anch here, as `return` is actually optimized.
  kExprBr, 0,
]);

// This tests that (1) we do not inline into try-catch blocks generally and
// (2) specifically for Turbolev, that we do not inline calls that are marked
// with `LazyDeoptOnThrow::kYes`.
// See https://crbug.com/447206453 and https://crrev.com/c/6991994 for more
// details.
// In short, Turbolev marks operations that can throw, but have never thrown
// so far with this flag, such that the JS function with the corresponding
// catch block do not have to generate code for the catch. That code is only
// generated on a lazy deopt, when the throwing operation actually throws.
// Since the Wasm function in this test does NOT throw, this test case
// covers the `LazyDeoptOnThrow::kYes` case.
// CHECK: Inlining JS-to-Wasm wrapper for Wasm function [{{[0-9]+}}] insideTryCatch of module {{.*}}
// CHECK-NEXT: Considering wasm function [{{[0-9]+}}] insideTryCatch of module {{.*}} for inlining
// CHECK-NEXT: - not inlining: call marked with LazyDeoptOnThrow::kYes
addTestcase('insideTryCatch', kSig_i_v, [], [
  ...wasmI32Const(0),
], (wasmExports) => {
  return function js_insideTryCatch(arg) {
    try {
      return wasmExports.insideTryCatch(arg);
    } catch (e) {
      return e.toString() + e.stack;
    }
  };
});

// See above. In contrast, this test case covers the `LazyDeoptOnThrow::kNo`
// case, since the Wasm body does trap (i.e., throws).
// CHECK: Inlining JS-to-Wasm wrapper for Wasm function [{{[0-9]+}}] trapNoInline of module {{.*}}
// CHECK-NEXT: Considering wasm function [{{[0-9]+}}] trapNoInline of module {{.*}} for inlining
// CHECK-NEXT: - not inlining: a current catch block is set
addTestcase('trapNoInline', kSig_v_v, [], [
  kExprUnreachable,
], (wasmExports) => {
  return function js_trapNoInline() {
    try {
      wasmExports.trapNoInline();
    } catch (e) {
      return e.toString() + e.stack;
    }
  };
});


// Compile and instantiate the Wasm module, define the caller JS functions,
// call with arguments defined above.
const wasmExports = builder.instantiate({}).exports;
for (const [ name, { wasmArguments, jsFunctionBuilder } ] of Object.entries(tests)) {
  print(`\nTest: ${name}`);

  const jsFunction = jsFunctionBuilder(wasmExports);

  const result = {};
  for (let i = 0; i < 2; i++) {
    const optimized = i == 1;
    if (optimized) {
      %OptimizeFunctionOnNextCall(jsFunction);
    } else {
      %PrepareFunctionForOptimization(jsFunction);
    }
    try {
      // Make sure we use the same call site for the optimized and unoptimized
      // call, such that the stack traces have the same locations in them
      // for the comparison below. Hence this for loop instead of simply two
      // calls to `jsFunction`.
      result[optimized] = jsFunction(...wasmArguments);
    } catch (e) {
      result[optimized] = e.toString() + e.stack;
    }
    if (optimized) {
      assertOptimized(jsFunction);
    } else {
      assertUnoptimized(jsFunction);
    }
  }

  print('Result:', prettyPrinted(result[true]));
  assertEquals(result[false], result[true]);
}
