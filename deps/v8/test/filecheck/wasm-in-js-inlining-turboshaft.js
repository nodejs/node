// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbolev --turbolev-inline-js-wasm-wrappers
// Flags: --turboshaft-wasm-in-js-inlining
// Flags: --allow-natives-syntax
// Flags: --no-always-sparkplug
// Flags: --turbofan --no-stress-maglev
// Only tier-up the test functions to get a cleaner and stable trace.
// Flags: --trace-turbo-inlining --turbo-filter='js_*'
// Concurrent inlining leads to additional traces.
// Flags: --no-concurrent-inlining
// Flags: --no-stress-concurrent-inlining

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");
d8.file.execute("test/mjsunit/mjsunit.js");

const builder = new WasmModuleBuilder();
const array = builder.addArray(kWasmI32);
const struct = builder.addStruct([makeField(kWasmI32, true)]);
const array8 = builder.addArray(kWasmI8);
const struct8 = builder.addStruct([makeField(kWasmI8, true)]);
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

// CHECK: Considering Wasm function [{{[0-9]+}}] empty of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('empty', kSig_v_v, [], []);
// CHECK: Considering Wasm function [{{[0-9]+}}] nop of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('nop', kSig_v_v, [], [kExprNop]);

// CHECK: Considering Wasm function [{{[0-9]+}}] i32Const of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('i32Const', kSig_i_v, [], [...wasmI32Const(42)]);
// CHECK: Considering Wasm function [{{[0-9]+}}] f32Const of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('f32Const', kSig_f_v, [], [...wasmF32Const(42.0)]);
// CHECK: Considering Wasm function [{{[0-9]+}}] f64Const of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('f64Const', kSig_d_v, [], [...wasmF64Const(42.0)]);

function addUnaryTestcase(op, wasmSignature, wasmArgument) {
  addTestcase(op, wasmSignature, [wasmArgument], [
    kExprLocalGet, 0,
    eval('kExpr' + op)
  ]);
}
// CHECK: Considering Wasm function [{{[0-9]+}}] I32Eqz of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addUnaryTestcase('I32Eqz', kSig_i_i, 0);
// CHECK: Considering Wasm function [{{[0-9]+}}] F32Abs of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addUnaryTestcase('F32Abs', kSig_f_f, 0);
// CHECK: Considering Wasm function [{{[0-9]+}}] F32Neg of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addUnaryTestcase('F32Neg', kSig_f_f, 0);
// CHECK: Considering Wasm function [{{[0-9]+}}] F32Sqrt of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addUnaryTestcase('F32Sqrt', kSig_f_f, 0);
// CHECK: Considering Wasm function [{{[0-9]+}}] F64Abs of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addUnaryTestcase('F64Abs', kSig_d_d, 0);
// CHECK: Considering Wasm function [{{[0-9]+}}] F64Neg of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addUnaryTestcase('F64Neg', kSig_d_d, 0);
// CHECK: Considering Wasm function [{{[0-9]+}}] F64Sqrt of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addUnaryTestcase('F64Sqrt', kSig_d_d, 0);
// CHECK: Considering Wasm function [{{[0-9]+}}] F64SConvertI32 of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addUnaryTestcase('F64SConvertI32', kSig_d_i, 0);
// CHECK: Considering Wasm function [{{[0-9]+}}] F64UConvertI32 of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addUnaryTestcase('F64UConvertI32', kSig_d_i, 0);
// CHECK: Considering Wasm function [{{[0-9]+}}] F32SConvertI32 of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addUnaryTestcase('F32SConvertI32', kSig_f_i, 0);
// CHECK: Considering Wasm function [{{[0-9]+}}] F32UConvertI32 of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addUnaryTestcase('F32UConvertI32', kSig_f_i, 0);
// CHECK: Considering Wasm function [{{[0-9]+}}] F32ConvertF64 of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addUnaryTestcase('F32ConvertF64', kSig_f_d, 0);
// CHECK: Considering Wasm function [{{[0-9]+}}] F64ConvertF32 of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addUnaryTestcase('F64ConvertF32', kSig_d_f, 0);
// CHECK: Considering Wasm function [{{[0-9]+}}] F32ReinterpretI32 of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addUnaryTestcase('F32ReinterpretI32', kSig_f_i, 0);
// CHECK: Considering Wasm function [{{[0-9]+}}] I32ReinterpretF32 of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addUnaryTestcase('I32ReinterpretF32', kSig_i_f, 0);
// CHECK: Considering Wasm function [{{[0-9]+}}] I32Clz of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addUnaryTestcase('I32Clz', kSig_i_i, 0);
// CHECK: Considering Wasm function [{{[0-9]+}}] I32SExtendI8 of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addUnaryTestcase('I32SExtendI8', kSig_i_i, 0);
// CHECK: Considering Wasm function [{{[0-9]+}}] I32SExtendI16 of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addUnaryTestcase('I32SExtendI16', kSig_i_i, 0);
// CHECK: Considering Wasm function [{{[0-9]+}}] RefIsNull of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addUnaryTestcase('RefIsNull', kSig_i_r, null);

function addBinaryTestcase(op, wasmSignature, wasmArgument0, wasmArgument1) {
  addTestcase(op, wasmSignature, [wasmArgument0, wasmArgument1], [
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    eval('kExpr' + op)
  ]);
}
// CHECK: Considering Wasm function [{{[0-9]+}}] I32Add of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('I32Add', kSig_i_ii, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] I32Sub of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('I32Sub', kSig_i_ii, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] I32Mul of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('I32Mul', kSig_i_ii, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] I32And of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('I32And', kSig_i_ii, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] I32Ior of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('I32Ior', kSig_i_ii, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] I32Xor of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('I32Xor', kSig_i_ii, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] I32Shl of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('I32Shl', kSig_i_ii, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] I32ShrS of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('I32ShrS', kSig_i_ii, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] I32ShrU of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('I32ShrU', kSig_i_ii, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] I32Ror of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('I32Ror', kSig_i_ii, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] I32Rol of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('I32Rol', kSig_i_ii, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] I32Eq of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('I32Eq', kSig_i_ii, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] I32Ne of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('I32Ne', kSig_i_ii, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] I32LtS of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('I32LtS', kSig_i_ii, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] I32LeS of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('I32LeS', kSig_i_ii, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] I32LtU of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('I32LtU', kSig_i_ii, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] I32LeU of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('I32LeU', kSig_i_ii, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] I32GtS of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('I32GtS', kSig_i_ii, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] I32GeS of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('I32GeS', kSig_i_ii, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] I32GtU of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('I32GtU', kSig_i_ii, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] I32GeU of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('I32GeU', kSig_i_ii, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] F32CopySign of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('F32CopySign', kSig_f_ff, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] F32Add of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('F32Add', kSig_f_ff, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] F32Sub of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('F32Sub', kSig_f_ff, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] F32Mul of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('F32Mul', kSig_f_ff, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] F32Div of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('F32Div', kSig_f_ff, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] F32Eq of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('F32Eq', kSig_i_ff, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] F32Ne of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('F32Ne', kSig_i_ff, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] F32Lt of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('F32Lt', kSig_i_ff, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] F32Le of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('F32Le', kSig_i_ff, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] F32Gt of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('F32Gt', kSig_i_ff, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] F32Ge of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('F32Ge', kSig_i_ff, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] F32Min of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('F32Min', kSig_f_ff, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] F32Max of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('F32Max', kSig_f_ff, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] F64Add of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('F64Add', kSig_d_dd, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] F64Sub of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('F64Sub', kSig_d_dd, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] F64Mul of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('F64Mul', kSig_d_dd, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] F64Div of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('F64Div', kSig_d_dd, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] F64Eq of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('F64Eq', kSig_i_dd, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] F64Ne of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('F64Ne', kSig_i_dd, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] F64Lt of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('F64Lt', kSig_i_dd, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] F64Le of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('F64Le', kSig_i_dd, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] F64Gt of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('F64Gt', kSig_i_dd, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] F64Ge of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('F64Ge', kSig_i_dd, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] F64Min of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('F64Min', kSig_d_dd, 3, 7);
// CHECK: Considering Wasm function [{{[0-9]+}}] F64Max of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addBinaryTestcase('F64Max', kSig_d_dd, 3, 7);

// This currently cannot be inlined when passing `eqref`s as an argument from JS
// because the JS-to-Wasm wrapper inlining bails out in that case.
// So go through through globals instead.
// CHECK: Considering Wasm function [{{[0-9]+}}] RefEq of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('RefEq', kSig_i_v, [], [
  kExprGlobalGet, globalEqRef.index,
  kExprGlobalGet, globalEqRef.index,
  kExprRefEq,
])

// Function arguments and locals.
// CHECK: Considering Wasm function [{{[0-9]+}}] passthroughI32 of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('passthroughI32', kSig_i_i, [13], [kExprLocalGet, 0]);
// We now support i64 in Wasm signatures when inlining the JS-to-Wasm wrapper on
// 32-bit architectures.
// CHECK: Considering JS-to-Wasm wrapper for Wasm function [{{[0-9]+}}] passthroughI64 of module {{.*}} for inlining
// CHECK-NEXT: - inlining wrapper
// CHECK: Considering Wasm function [{{[0-9]+}}] passthroughI64 of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('passthroughI64', kSig_l_l, [13n], [kExprLocalGet, 0]);
// CHECK: Considering Wasm function [{{[0-9]+}}] localTee of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('localTee', kSig_i_i, [42], [
  kExprLocalGet, 0,
  ...wasmI32Const(7),
  kExprLocalTee, 1,
  kExprI32Add,
  kExprLocalGet, 1,
  kExprI32Add,
]).addLocals(kWasmI32, 1);
// CHECK: Considering Wasm function [{{[0-9]+}}] localSwap of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
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

// CHECK: Considering Wasm function [{{[0-9]+}}] globalSetGet of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('globalSetGet', kSig_i_i, [42], [
  kExprLocalGet, 0,
  kExprGlobalSet, globalI32.index,
  kExprGlobalGet, globalI32.index,
  kExprGlobalGet, globalI32.index,
  kExprI32Add,
  kExprGlobalGet, globalI32.index,
  kExprI32Add,
]);

// CHECK: Considering Wasm function [{{[0-9]+}}] anyConvertExtern of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('anyConvertExtern', makeSig([kWasmExternRef], [kWasmI32]), [{}], [
  kExprLocalGet, 0,
  kGCPrefix, kExprAnyConvertExtern,
  kExprRefIsNull,
]);

// CHECK: Considering Wasm function [{{[0-9]+}}] anyConvertExternLargeSmi of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
// On 64-bit platforms without pointer compression, Smis are 32-bit.
// 1073741824 == 2^30 == kInt31MaxValue + 1 exceeds the 31-bit range of Wasm's
// i31ref, forcing any.convert_extern to allocate a HeapNumber via a builtin
// call, covering that specific Turboshaft lowering path.
addTestcase('anyConvertExternLargeSmi', makeSig([kWasmExternRef], [kWasmI32]), [1073741824], [
  kExprLocalGet, 0,
  kGCPrefix, kExprAnyConvertExtern,
  kExprRefIsNull,
]);

// CHECK: Considering Wasm function [{{[0-9]+}}] externConvertAny of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('externConvertAny', makeSig([], [kWasmExternRef]), [], [
  kExprGlobalGet, globalEqRef.index,
  kGCPrefix, kExprExternConvertAny,
]);

// CHECK: Considering JS-to-Wasm wrapper for Wasm function [{{[0-9]+}}] unreachable of module {{.*}} for inlining
// CHECK-NEXT: - inlining wrapper
// CHECK-NEXT: Considering Wasm function [{{[0-9]+}}] unreachable of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('unreachable', kSig_v_v, [], [
  kExprUnreachable,
]);

// CHECK: Considering Wasm function [{{[0-9]+}}] empty of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
// CHECK: Considering Wasm function [{{[0-9]+}}] sameModuleMultipleFunctions of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('sameModuleMultipleFunctions', kSig_v_v, [], [], (wasmExports) => {
  return function js_sameModuleMultipleFunctions() {
    wasmExports.empty();
    wasmExports.sameModuleMultipleFunctions();
  };
});

const globalNullArray = builder.addGlobal(wasmRefNullType(array), true, false);

// This will throw and should have the right error message and stack trace.
// CHECK: Considering Wasm function [{{[0-9]+}}] arrayLenNull of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('arrayLenNull', kSig_i_v, [], [
  kExprGlobalGet, globalNullArray.index,
  kGCPrefix, kExprArrayLen,
]);

// -----------------------------------------------------------------------------
// Casts and type checks.
// -----------------------------------------------------------------------------

// We statically know that this cast will not succeed (because it's casting an
// array to a struct), so this is optimized into a null check only.
// CHECK: Considering Wasm function [{{[0-9]+}}] assertNullTypecheck of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('assertNullTypecheck', kSig_i_v, [], [
  kExprGlobalGet, globalNullArray.index,
  kGCPrefix, kExprRefCastNull, struct,
  kExprRefIsNull,
]);

// We statically know that this cast will succeed, so this is also a null check.
// CHECK: Considering Wasm function [{{[0-9]+}}] assertNotNullTypecheck of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('assertNotNullTypecheck', kSig_i_v, [], [
  kExprGlobalGet, globalNullArray.index,
  kGCPrefix, kExprRefCast, array,
  kGCPrefix, kExprArrayLen,
]);

// We statically know that this test will succeed if not null.
// CHECK: Considering Wasm function [{{[0-9]+}}] refTestAlwaysSucceedsButNull of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('refTestAlwaysSucceedsButNull', kSig_i_v, [], [
  kExprGlobalGet, globalNullArray.index,
  kGCPrefix, kExprRefTest, array,
]);

const globalAnyRef = builder.addGlobal(kWasmAnyRef, true, false);

builder.addFunction('initGlobalAnyRefArray', makeSig([], [])).addBody([
  ...wasmI32Const(42),
  kGCPrefix, kExprArrayNewDefault, array,
  kExprGlobalSet, globalAnyRef.index,
]).exportFunc();

// CHECK: Considering Wasm function [{{[0-9]+}}] refCast of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('refCast', makeSig([], [kWasmI32]), [], [
  kExprGlobalGet, globalAnyRef.index,
  kGCPrefix, kExprRefCast, array,
  kGCPrefix, kExprArrayLen,
], (wasmExports) => {
  wasmExports.initGlobalAnyRefArray();
  return function js_refCast() {
    return wasmExports.refCast();
  };
});

builder.addFunction('initGlobalAnyRefStruct', makeSig([], [])).addBody([
  kGCPrefix, kExprStructNewDefault, struct,
  kExprGlobalSet, globalAnyRef.index,
]).exportFunc();

// This will throw because we cannot cast a struct to an array.
// CHECK: Considering Wasm function [{{[0-9]+}}] refCastFail of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('refCastFail', makeSig([], [kWasmI32]), [], [
  kExprGlobalGet, globalAnyRef.index,
  kGCPrefix, kExprRefCast, array,
  kGCPrefix, kExprArrayLen,
], (wasmExports) => {
  wasmExports.initGlobalAnyRefStruct();
  return function js_refCastFail() {
    return wasmExports.refCastFail();
  };
});

// CHECK: Considering Wasm function [{{[0-9]+}}] refCastAbstract of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('refCastAbstract', makeSig([], [kWasmI32]), [], [
  kExprGlobalGet, globalAnyRef.index,
  kGCPrefix, kExprRefCast, kEqRefCode,
  kExprRefIsNull,
], (wasmExports) => {
  wasmExports.initGlobalAnyRefStruct();
  return function js_refCastAbstract() {
    return wasmExports.refCastAbstract();
  };
});

// CHECK: Considering Wasm function [{{[0-9]+}}] refCastAbstractFail of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('refCastAbstractFail', makeSig([], [kWasmI32]), [], [
  kExprGlobalGet, globalAnyRef.index,
  kGCPrefix, kExprRefCast, kArrayRefCode,
  kExprRefIsNull,
], (wasmExports) => {
  wasmExports.initGlobalAnyRefStruct();
  return function js_refCastAbstractFail() {
    return wasmExports.refCastAbstractFail();
  };
});

// -----------------------------------------------------------------------------
// Array operations.
// -----------------------------------------------------------------------------

// CHECK: Considering Wasm function [{{[0-9]+}}] arrayLenParam of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
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

const globalArray = builder.addGlobal(wasmRefNullType(array), true, false);
const globalArray8 = builder.addGlobal(wasmRefNullType(array8), true, false);

builder.addFunction('initGlobalArray', makeSig([], [])).addBody([
  ...wasmI32Const(42),
  kGCPrefix, kExprArrayNewDefault, array,
  kExprGlobalSet, globalArray.index,
]).exportFunc();

// CHECK: Considering Wasm function [{{[0-9]+}}] arrayLenGlobal of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
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

// CHECK: Considering Wasm function [{{[0-9]+}}] arraySet of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('arraySet', kSig_v_v, [], [
  kExprGlobalGet, globalArray.index,
  ...wasmI32Const(0),
  ...wasmI32Const(23),
  kGCPrefix, kExprArraySet, array,
], (wasmExports) => {
  wasmExports.initGlobalArray();
  return function js_arraySet() {
    wasmExports.arraySet();
  };
});

// CHECK: Considering Wasm function [{{[0-9]+}}] arrayGet of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('arrayGet', kSig_i_v, [], [
  kExprGlobalGet, globalArray.index,
  ...wasmI32Const(0),
  kGCPrefix, kExprArrayGet, array,
], (wasmExports) => {
  wasmExports.arraySet();
  return function js_arrayGet() {
    return wasmExports.arrayGet();
  };
});

// CHECK: Considering Wasm function [{{[0-9]+}}] arrayGetParam of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('arrayGetParam', makeSig([kWasmExternRef], [kWasmI32]), [null], [
  kExprLocalGet, 0,
  kGCPrefix, kExprAnyConvertExtern,
  kGCPrefix, kExprRefCastNull, array,
  ...wasmI32Const(0),
  kGCPrefix, kExprArrayGet, array,
], (wasmExports) => {
  const arr = wasmExports.createArray(42);
  return function js_arrayGetParam() {
    return wasmExports.arrayGetParam(arr);
  };
});

builder.addFunction('initGlobalArray8', makeSig([], [])).addBody([
  ...wasmI32Const(42),
  kGCPrefix, kExprArrayNewDefault, array8,
  kExprGlobalSet, globalArray8.index,
]).exportFunc();

// CHECK: Considering Wasm function [{{[0-9]+}}] arrayGetS of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('arrayGetS', kSig_i_v, [], [
  kExprGlobalGet, globalArray8.index,
  ...wasmI32Const(0),
  kGCPrefix, kExprArrayGetS, array8,
], (wasmExports) => {
  wasmExports.initGlobalArray8();
  return function js_arrayGetS() {
    return wasmExports.arrayGetS();
  };
});

// CHECK: Considering Wasm function [{{[0-9]+}}] arrayGetU of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('arrayGetU', kSig_i_v, [], [
  kExprGlobalGet, globalArray8.index,
  ...wasmI32Const(0),
  kGCPrefix, kExprArrayGetU, array8,
]);

// CHECK: Considering Wasm function [{{[0-9]+}}] arrayGetOutOfBounds of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('arrayGetOutOfBounds', kSig_i_v, [], [
  kExprGlobalGet, globalArray.index,
  ...wasmI32Const(100),
  kGCPrefix, kExprArrayGet, array,
]);

// CHECK: Considering Wasm function [{{[0-9]+}}] arrayGetNull of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('arrayGetNull', kSig_i_v, [], [
  kExprGlobalGet, globalNullArray.index,
  ...wasmI32Const(0),
  kGCPrefix, kExprArrayGet, array,
]);

// CHECK: Considering Wasm function [{{[0-9]+}}] arraySetOutOfBounds of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('arraySetOutOfBounds', kSig_v_v, [], [
  kExprGlobalGet, globalArray.index,
  ...wasmI32Const(100),
  ...wasmI32Const(23),
  kGCPrefix, kExprArraySet, array,
]);

// CHECK: Considering Wasm function [{{[0-9]+}}] arraySetNull of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('arraySetNull', kSig_v_v, [], [
  kExprGlobalGet, globalNullArray.index,
  ...wasmI32Const(0),
  ...wasmI32Const(23),
  kGCPrefix, kExprArraySet, array,
]);

// -----------------------------------------------------------------------------
// Struct operations.
// -----------------------------------------------------------------------------

const globalStruct = builder.addGlobal(wasmRefNullType(struct), true, false);
const globalStruct8 = builder.addGlobal(wasmRefNullType(struct8), true, false);
const globalNullStruct = builder.addGlobal(wasmRefNullType(struct), true, false);

builder.addFunction('initGlobalStruct', makeSig([], [])).addBody([
  ...wasmI32Const(42),
  kGCPrefix, kExprStructNew, struct,
  kExprGlobalSet, globalStruct.index,
]).exportFunc();

builder.addFunction('initGlobalStruct8', makeSig([], [])).addBody([
  ...wasmI32Const(-1),
  kGCPrefix, kExprStructNew, struct8,
  kExprGlobalSet, globalStruct8.index,
]).exportFunc();

// CHECK: Considering Wasm function [{{[0-9]+}}] structSet of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addTestcase('structSet', kSig_v_v, [], [
  kExprGlobalGet, globalStruct.index,
  ...wasmI32Const(23),
  kGCPrefix, kExprStructSet, struct, 0,
], (wasmExports) => {
  wasmExports.initGlobalStruct();
  return function js_structSet() {
    wasmExports.structSet();
  };
});

// CHECK: Considering Wasm function [{{[0-9]+}}] structGet of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addTestcase('structGet', kSig_i_v, [], [
  kExprGlobalGet, globalStruct.index,
  kGCPrefix, kExprStructGet, struct, 0,
], (wasmExports) => {
  wasmExports.structSet();
  return function js_structGet() {
    return wasmExports.structGet();
  };
});

// CHECK: Considering Wasm function [{{[0-9]+}}] structGetParam of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
addTestcase('structGetParam', makeSig([kWasmExternRef], [kWasmI32]), [null], [
  kExprLocalGet, 0,
  kGCPrefix, kExprAnyConvertExtern,
  kGCPrefix, kExprRefCastNull, struct,
  kGCPrefix, kExprStructGet, struct, 0,
], (wasmExports) => {
  const str = wasmExports.createStruct();
  return function js_structGetParam() {
    return wasmExports.structGetParam(str);
  };
});

// CHECK: Considering Wasm function [{{[0-9]+}}] structGetS of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addTestcase('structGetS', kSig_i_v, [], [
  kExprGlobalGet, globalStruct8.index,
  kGCPrefix, kExprStructGetS, struct8, 0,
], (wasmExports) => {
  wasmExports.initGlobalStruct8();
  return function js_structGetS() {
    return wasmExports.structGetS();
  };
});

// CHECK: Considering Wasm function [{{[0-9]+}}] structGetU of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addTestcase('structGetU', kSig_i_v, [], [
  kExprGlobalGet, globalStruct8.index,
  kGCPrefix, kExprStructGetU, struct8, 0,
]);

// CHECK: Considering Wasm function [{{[0-9]+}}] structGetNull of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addTestcase('structGetNull', kSig_i_v, [], [
  kExprGlobalGet, globalNullStruct.index,
  kGCPrefix, kExprStructGet, struct, 0,
]);

// CHECK: Considering Wasm function [{{[0-9]+}}] structSetNull of module {{.*}} for inlining
// CHECK-NEXT: - inlining
addTestcase('structSetNull', kSig_v_v, [], [
  kExprGlobalGet, globalNullStruct.index,
  ...wasmI32Const(23),
  kGCPrefix, kExprStructSet, struct, 0,
]);


// =============================================================================
// Testcases that we (currently) do not inline (the JS-to-Wasm wrapper or body):
// =============================================================================

// TODO(dlehmann,353475584): `createArray` requires allocation, which might get
// complicated with allocation folding, so that's not in the MVP yet.
// CHECK: Considering Wasm function [{{[0-9]+}}] createArray of module {{.*}} for inlining
// CHECK-NEXT: - not inlining: unsupported operation: array.new_default
addTestcase('createArray', makeSig([kWasmI32], [kWasmExternRef]), [42], [
  kExprLocalGet, 0,
  kGCPrefix, kExprArrayNewDefault, array,
  kGCPrefix, kExprExternConvertAny,
]);

// CHECK: Considering Wasm function [{{[0-9]+}}] createStruct of module {{.*}} for inlining
// CHECK-NEXT: - not inlining: unsupported operation: struct.new_default
addTestcase('createStruct', makeSig([], [kWasmExternRef]), [], [
  kGCPrefix, kExprStructNewDefault, struct,
  kGCPrefix, kExprExternConvertAny,
]);

// TODO(dlehmann,353475584): We don't support i64 ops yet, since that requires
// `Int64LoweringReducer`, which isn't compatible with the JS pipeline yet.
// Also, there is no wrapper inlining for i64 on 32-bit architectures right
// now, so pass as i32 on the boundary.
// CHECK: Considering Wasm function [{{[0-9]+}}] i64Add of module {{.*}} for inlining
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
// CHECK-NOT: - inlining wrapper
// CHECK-NOT: - inlining Wasm function
// CHECK: Result:
addTestcase('multiValue', kSig_ii_v, [], [
  ...wasmI32Const(3),
  ...wasmI32Const(7),
]);

// TODO(dlehmann,353475584): Support control-flow in the inlinee (much later).
// CHECK: Considering Wasm function [{{[0-9]+}}] brNoInline of module {{.*}} for inlining
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
// CHECK: Considering JS-to-Wasm wrapper for Wasm function [{{[0-9]+}}] insideTryCatch of module {{.*}} for inlining
// CHECK-NEXT: - inlining wrapper
// CHECK-NEXT: Considering Wasm function [{{[0-9]+}}] insideTryCatch of module {{.*}} for inlining
// CHECK-NEXT: - not inlining: call marked with LazyDeoptOnThrow::kYes
addTestcase('insideTryCatch', kSig_i_v, [], [
  ...wasmI32Const(0),
], (wasmExports) => {
  return function js_insideTryCatch() {
    try {
      return wasmExports.insideTryCatch();
    } catch (e) {
      return e.toString() + e.stack;
    }
  };
});

// See above. In contrast, this test case covers the `LazyDeoptOnThrow::kNo`
// case, since the Wasm body does trap (i.e., throws).
// CHECK: Considering JS-to-Wasm wrapper for Wasm function [{{[0-9]+}}] trapNoInline of module {{.*}} for inlining
// CHECK-NEXT: - inlining wrapper
// CHECK-NEXT: Considering Wasm function [{{[0-9]+}}] trapNoInline of module {{.*}} for inlining
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

const builder2 = new WasmModuleBuilder();
builder2.addFunction('noop2', kSig_v_v).addBody([]).exportFunc();
const wasmExports2 = builder2.instantiate({}).exports;

// CHECK: Considering JS-to-Wasm wrapper for Wasm function [{{[0-9]+}}] noop2 of module {{.*}} for inlining
// CHECK-NEXT: - inlining wrapper
// CHECK-NEXT: Considering JS-to-Wasm wrapper for Wasm function [{{[0-9]+}}] multiModule of module {{.*}} for inlining
// CHECK-NEXT: - not inlining: already inlining from another Wasm module
// CHECK: Considering Wasm function [{{[0-9]+}}] noop2 of module {{.*}} for inlining
// CHECK-NEXT: - inlining Wasm function
// CHECK-NOT: Considering Wasm function [{{[0-9]+}}] multiModule of module {{.*}} for inlining
addTestcase('multiModule', kSig_v_v, [], [], (wasmExports) => {
  return function js_multiModule() {
    wasmExports2.noop2();
    wasmExports.multiModule();
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
