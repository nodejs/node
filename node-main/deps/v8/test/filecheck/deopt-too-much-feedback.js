// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --wasm-deopt --liftoff --wasm-inlining
// Flags: --no-jit-fuzzing --trace-deopt --trace-wasm-compilation-times
// Flags: --deopt-every-n-times=0 --trace-wasm-inlining

d8.file.execute("test/mjsunit/mjsunit.js");
d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let createModule = (function() {
  return function createModule() {
    var builder = new WasmModuleBuilder();
    let funcRefT = builder.addType(kSig_i_ii);

    builder.addFunction("add", funcRefT)
      .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Add])
      .exportFunc();
    builder.addFunction("mul", funcRefT)
      .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Mul])
      .exportFunc();
    builder.addFunction("sub", funcRefT)
      .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Sub])
      .exportFunc();
    builder.addFunction("first", funcRefT)
      .addBody([kExprLocalGet, 0])
      .exportFunc();
    builder.addFunction("second", funcRefT)
      .addBody([kExprLocalGet, 1])
      .exportFunc();
    builder.addFunction("equals", funcRefT)
      .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Eq,])
      .exportFunc();

    let mainSig =
      makeSig([kWasmI32, kWasmI32, wasmRefType(funcRefT)], [kWasmI32]);
    builder.addFunction("main", mainSig)
      .addLocals(kWasmI32, 1)
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprLocalGet, 2,
        kExprCallRef, funcRefT,
        kExprLocalTee, 3,
        // If the result is 0, repeat the call, so that we have a second deopt
        // point.
        kExprI32Eqz,
        kExprIf, kWasmVoid,
          kExprI32Const, 0,
          kExprI32Const, 0,
          kExprLocalGet, 2,
          kExprCallRef, funcRefT,
          kExprDrop,
        kExprEnd,
        kExprLocalGet, 3,
    ]).exportFunc();
    return builder.toModule();
  }
})();

// Test a scenario where each instance only has polymorphic call feedback but
// the instances combined sum up to more feedback slots than the maximum
// polymorphism (4).
(function TestCollectiveFeedbackMoreThanMaxPolymorphism() {
  let module = createModule();

  let wasm1 = new WebAssembly.Instance(module).exports;
  // Collect polymorphic feedback for wasm1.
  assertEquals(42, wasm1.main(30, 12, wasm1.add));
  assertEquals(360, wasm1.main(30, 12, wasm1.mul));
  assertEquals(18, wasm1.main(30, 12, wasm1.sub));
  %WasmTierUpFunction(wasm1.main);
  // Trigger deopt and collect polymorphic feedback for wasm2.
  let wasm2 = new WebAssembly.Instance(module).exports;
  // CHECK: function 6: call #0 inlineable (polymorphic 3)
  // CHECK: function 6: Speculatively inlining call_ref #0, case #0, to function 0
  // CHECK: function 6: Speculatively inlining call_ref #0, case #1, to function 1
  // CHECK: function 6: Speculatively inlining call_ref #0, case #2, to function 2
  // CHECK: Compiled function [[NATIVE_MODULE:(0x)?[0-9a-fA-F]+]]#6 using TurboFan
  assertTrue(%IsTurboFanFunction(wasm2.main));
  assertEquals(30, wasm2.main(30, 12, wasm2.first));
  // Deopt due to inlined target mismatch.
  // CHECK-NEXT: bailout (kind: deopt-eager, reason: wrong call target, type: Wasm): begin. deoptimizing main, function index 6, bytecode offset 144
  assertFalse(%IsTurboFanFunction(wasm2.main));
  // Returning zero here means that the second call_ref in the main function now
  // has feedback for "second".
  assertEquals(0, wasm2.main(30, 0, wasm2.second));

  // We have 5 different call targets, so the call is megamorphic. (The
  // per-instance feedback for both instances is polymorphic.)
  // CHECK: function 6: call #0: megamorphic
  // CHECK: function 6: call #1 inlineable (monomorphic)
  // CHECK: function 6: Speculatively inlining call_ref #1, case #0, to function 4
  // CHECK: Compiled function [[NATIVE_MODULE]]#6 using TurboFan
  %WasmTierUpFunction(wasm2.main);
  for (let wasm of [wasm1, wasm2]) {
    assertEquals(42, wasm.main(30, 12, wasm.add));
    assertEquals(360, wasm.main(30, 12, wasm.mul));
    assertEquals(18, wasm.main(30, 12, wasm.sub));
    assertEquals(30, wasm.main(30, 12, wasm.first));
    assertEquals(12, wasm.main(30, 12, wasm.second));
    // New targets don't trigger a de-opt either (as long as they don't return
    // zero which would cause them executing the second call_ref).
    assertEquals(1, wasm.main(30, 30, wasm.equals));
    assertTrue(%IsTurboFanFunction(wasm.main));
  }

  let wasm3 = new WebAssembly.Instance(module).exports;
  assertEquals(42, wasm3.main(30, 12, wasm3.add));
  assertTrue(%IsTurboFanFunction(wasm3.main));

  // CHECK: Trigger deopt on second call_ref for wasm3
  print("Trigger deopt on second call_ref for wasm3");
  // Trigger deoptimization on second call_ref which so far has only seen the
  // "second" function.
  assertEquals(0, wasm3.main(12, 12, wasm3.sub));
  // CHECK: bailout (kind: deopt-eager, reason: wrong call target, type: Wasm): begin. deoptimizing main, function index 6, bytecode offset 157
  assertFalse(%IsTurboFanFunction(wasm3.main));
  // Now that we are back in liftoff, collect feedback for equals on the first
  // call_ref as well.
  assertEquals(0, wasm3.main(30, 12, wasm3.equals));
  // CHECK: function 6: call #0: megamorphic
  // CHECK: Compiled function [[NATIVE_MODULE]]#6 using TurboFan
  // CHECK-NOT: bailout (kind: deopt-eager, reason: wrong call target, type: Wasm)
  %WasmTierUpFunction(wasm3.main);

  // The first call is still megamorphic, so we can call all functions without
  // triggering a deopt (as long as the result isn't zero).
  for (let wasm of [wasm1, wasm2, wasm3]) {
    assertEquals(42, wasm.main(30, 12, wasm.add));
    assertEquals(360, wasm.main(30, 12, wasm.mul));
    assertEquals(18, wasm.main(30, 12, wasm.sub));
    assertEquals(30, wasm.main(30, 12, wasm.first));
    assertEquals(12, wasm.main(30, 12, wasm.second));
    assertEquals(1, wasm.main(30, 30, wasm.equals));
    assertTrue(%IsTurboFanFunction(wasm.main));
  }
})();
