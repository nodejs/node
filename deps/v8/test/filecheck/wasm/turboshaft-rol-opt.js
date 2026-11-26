// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --trace-turbo-graph --no-liftoff --no-wasm-lazy-compilation --wasm-opt

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

function compile(name, sig, body) {
  const builder = new WasmModuleBuilder();
  builder.addFunction(name, sig).addBody(body);
  builder.instantiate();
}

// CHECK-LABEL: Begin compiling method rol32By0 using Turboshaft
// CHECK-LABEL: V8.TFTurboshaftWasmOptimize
// CHECK-NOT: Rotate
// CHECK: Return(
compile('rol32By0', kSig_i_i, [
  kExprLocalGet, 0, ...wasmI32Const(0), kExprI32Rol,
]);

// CHECK-LABEL: Begin compiling method rol32By32 using Turboshaft
// CHECK-LABEL: V8.TFTurboshaftWasmOptimize
// CHECK-NOT: Rotate
// CHECK: Return(
compile('rol32By32', kSig_i_i, [
  kExprLocalGet, 0, ...wasmI32Const(32), kExprI32Rol,
]);

// CHECK-LABEL: Begin compiling method rol64By0 using Turboshaft
// CHECK-LABEL: V8.TFTurboshaftWasmOptimize
// CHECK-NOT: Rotate
// CHECK: Return(
compile('rol64By0', kSig_l_l, [
  kExprLocalGet, 0, ...wasmI64Const(0), kExprI64Rol,
]);

// CHECK-LABEL: Begin compiling method rol64By64 using Turboshaft
// CHECK-LABEL: V8.TFTurboshaftWasmOptimize
// CHECK-NOT: Rotate
// CHECK: Return(
compile('rol64By64', kSig_l_l, [
  kExprLocalGet, 0, ...wasmI64Const(64), kExprI64Rol,
]);
