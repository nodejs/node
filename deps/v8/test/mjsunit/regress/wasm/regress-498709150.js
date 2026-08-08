// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbolev --turbolev-inline-js-wasm-wrappers
// Flags: --turboshaft-wasm-in-js-inlining
// Flags: --allow-natives-syntax

// When a JS-to-Wasm wrapper call is inside a try/catch, Maglev emits
// LazyDeoptOnThrow::kYes on the call. The conversion builtins (e.g.,
// BigIntToI64) can throw for wrong types, but previously the inlined wrapper
// didn't attach a frame state to these builtin calls. This meant the
// deoptimizer couldn't find deoptimization info when walking the stack after
// a throw, causing a crash ("Missing deoptimization information").
//
// The fix:
// - For i32/f32/f64: eager deopt if not Smi/HeapNumber (no builtin call).
// - For i64: propagate the eager frame state + LazyDeoptOnThrow to the
//   BigIntToI64 builtin call so it can properly lazy-deopt on throw.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Test i64 inside try/catch — the original crash scenario.
(function testI64InTryCatch() {
  var builder = new WasmModuleBuilder();
  builder.addFunction("identity", kSig_l_l)
    .addBody([kExprLocalGet, 0])
    .exportAs("identity");

  let instance = builder.instantiate();

  function callWasm(x) {
    try {
      return instance.exports.identity(x);
    } catch (e) {
      return e;
    }
  }

  %PrepareFunctionForOptimization(callWasm);
  assertEquals(0n, callWasm(0n));
  assertEquals(42n, callWasm(42n));
  %OptimizeFunctionOnNextCall(callWasm);
  assertEquals(7n, callWasm(7n));

  // Wrong types should throw and be caught, not crash.
  let e1 = callWasm("not a bigint");
  assertTrue(e1 instanceof SyntaxError, "string should throw SyntaxError");

  let e2 = callWasm(42);
  assertTrue(e2 instanceof TypeError, "number should throw TypeError");

  // JSReceiver with valueOf returning BigInt: ToBigInt calls valueOf, succeeds.
  assertEquals(1n, callWasm({ valueOf() { return 1n; } }));

  let e4 = callWasm(undefined);
  assertTrue(e4 instanceof TypeError, "undefined should throw TypeError");
})();

// Test i32 inside try/catch — should eagerly deopt on non-number, then
// the interpreter re-executes the call which throws via the non-inlined wrapper.
(function testI32InTryCatch() {
  var builder = new WasmModuleBuilder();
  builder.addFunction("identity", kSig_i_i)
    .addBody([kExprLocalGet, 0])
    .exportAs("identity");

  let instance = builder.instantiate();

  function callWasm(x) {
    try {
      return instance.exports.identity(x);
    } catch (e) {
      return e;
    }
  }

  %PrepareFunctionForOptimization(callWasm);
  assertEquals(1, callWasm(1));
  assertEquals(3, callWasm(3));
  %OptimizeFunctionOnNextCall(callWasm);
  assertEquals(5, callWasm(5));

  // BigInt throws TypeError in non-inlined wrapper.
  let e1 = callWasm(2n);
  assertTrue(e1 instanceof TypeError, "bigint should throw TypeError");

  // Objects are handled by eager deopt -> interpreter -> valueOf called.
  assertEquals(7, callWasm({ valueOf() { return 7; } }));
})();

// Test f64 inside try/catch.
(function testF64InTryCatch() {
  var builder = new WasmModuleBuilder();
  builder.addFunction("identity", kSig_d_d)
    .addBody([kExprLocalGet, 0])
    .exportAs("identity");

  let instance = builder.instantiate();

  function callWasm(x) {
    try {
      return instance.exports.identity(x);
    } catch (e) {
      return e;
    }
  }

  %PrepareFunctionForOptimization(callWasm);
  callWasm(1.5);
  callWasm(3.0);
  %OptimizeFunctionOnNextCall(callWasm);
  assertEquals(5.0, callWasm(5.0));

  // BigInt throws TypeError in non-inlined wrapper.
  let e1 = callWasm(2n);
  assertTrue(e1 instanceof TypeError, "bigint should throw TypeError");

  // Objects: eager deopt -> interpreter -> valueOf.
  assertEquals(7, callWasm({ valueOf() { return 7; } }));
})();
