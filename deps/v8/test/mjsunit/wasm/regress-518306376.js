// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff

// Regression test for v8:518306376.
//
// Select instructions whose condition is produced by a floating point
// comparison. The condition operands may be folded into an ImmediateOperand
// by the instruction selector, in which case the code generator must not
// derive the kind of condition from the operands but from the opcode.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const kTrueValue = 11;
const kFalseValue = 22;

// Builds
//   (func (param $ty) (result i64)
//     (select (i64.const 11) (i64.const 22) ($op $const ...) ))
// with the constant as the left operand of the comparison.
function SelectWithConstOnLeft(op, constBytes, ty) {
  const builder = new WasmModuleBuilder();
  builder.addFunction('f', makeSig([ty], [kWasmI64]))
      .addBody([
        ...wasmI64Const(kTrueValue), ...wasmI64Const(kFalseValue),
        ...constBytes,  //
        kExprLocalGet, 0,
        op,
        kExprSelect,
      ])
      .exportFunc();
  return builder.instantiate().exports.f;
}

// Same, but with the constant as the right operand of the comparison.
function SelectWithConstOnRight(op, constBytes, ty) {
  const builder = new WasmModuleBuilder();
  builder.addFunction('f', makeSig([ty], [kWasmI64]))
      .addBody([
        ...wasmI64Const(kTrueValue), ...wasmI64Const(kFalseValue),
        kExprLocalGet, 0,
        ...constBytes,  //
        op,
        kExprSelect,
      ])
      .exportFunc();
  return builder.instantiate().exports.f;
}

const kF64Zero = wasmF64Const(0.0);
const kF32Zero = wasmF32Const(0.0);

(function TestFloat64LessThanZeroOnLeft() {
  const f = SelectWithConstOnLeft(kExprF64Lt, kF64Zero, kWasmF64);
  assertEquals(BigInt(kFalseValue), f(0.0));  // 0.0 < 0.0 is false
  assertEquals(BigInt(kTrueValue), f(1.0));   // 0.0 < 1.0 is true
  assertEquals(BigInt(kFalseValue), f(-1.0));  // 0.0 < -1.0 is false
  assertEquals(BigInt(kFalseValue), f(NaN));
})();

(function TestFloat64LessThanOrEqualZeroOnLeft() {
  const f = SelectWithConstOnLeft(kExprF64Le, kF64Zero, kWasmF64);
  assertEquals(BigInt(kTrueValue), f(0.0));
  assertEquals(BigInt(kTrueValue), f(1.0));
  assertEquals(BigInt(kFalseValue), f(-1.0));
  assertEquals(BigInt(kFalseValue), f(NaN));
})();

(function TestFloat64GreaterThanZeroOnLeft() {
  // The graph builder commutes kExprF64Gt into F64Lt with swapped operands,
  // so this covers the "constant on the right" path for lt.
  const f = SelectWithConstOnLeft(kExprF64Gt, kF64Zero, kWasmF64);
  assertEquals(BigInt(kFalseValue), f(0.0));
  assertEquals(BigInt(kFalseValue), f(1.0));
  assertEquals(BigInt(kTrueValue), f(-1.0));
})();

(function TestFloat64GreaterThanOrEqualZeroOnLeft() {
  const f = SelectWithConstOnLeft(kExprF64Ge, kF64Zero, kWasmF64);
  assertEquals(BigInt(kTrueValue), f(0.0));
  assertEquals(BigInt(kFalseValue), f(1.0));
  assertEquals(BigInt(kTrueValue), f(-1.0));
})();

(function TestFloat64EqualZeroOnLeft() {
  const f = SelectWithConstOnLeft(kExprF64Eq, kF64Zero, kWasmF64);
  assertEquals(BigInt(kTrueValue), f(0.0));
  assertEquals(BigInt(kFalseValue), f(1.0));
})();

(function TestFloat64EqualZeroOnRight() {
  const f = SelectWithConstOnRight(kExprF64Eq, kF64Zero, kWasmF64);
  assertEquals(BigInt(kTrueValue), f(0.0));
  assertEquals(BigInt(kFalseValue), f(1.0));
})();

(function TestFloat64LessThanNonZeroConstOnLeft() {
  const f = SelectWithConstOnLeft(kExprF64Lt, wasmF64Const(1.0), kWasmF64);
  assertEquals(BigInt(kTrueValue), f(2.0));   // 1.0 < 2.0 is true
  assertEquals(BigInt(kFalseValue), f(0.5));  // 1.0 < 0.5 is false
})();

(function TestFloat32LessThanZeroOnLeft() {
  const f = SelectWithConstOnLeft(kExprF32Lt, kF32Zero, kWasmF32);
  assertEquals(BigInt(kFalseValue), f(0.0));
  assertEquals(BigInt(kTrueValue), f(1.0));
  assertEquals(BigInt(kFalseValue), f(-1.0));
})();

(function TestFloat32LessThanOrEqualZeroOnLeft() {
  const f = SelectWithConstOnLeft(kExprF32Le, kF32Zero, kWasmF32);
  assertEquals(BigInt(kTrueValue), f(0.0));
  assertEquals(BigInt(kFalseValue), f(-1.0));
})();
