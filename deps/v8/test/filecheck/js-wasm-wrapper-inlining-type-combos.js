// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbolev --turbolev-inline-js-wasm-wrappers
// Flags: --turboshaft-wasm-in-js-inlining
// Flags: --allow-natives-syntax
// Flags: --trace-turbo-inlining
// Flags: --turbofan --no-stress-maglev
// Concurrent inlining leads to additional traces.
// Flags: --no-concurrent-inlining
// Flags: --no-stress-concurrent-inlining

// Comprehensive test for JS-to-Wasm wrapper inlining with all combinations of:
// - Wasm parameter types: i32, i64, f32, f64
// - JS input values: Smi, HeapNumber, BigInt, Symbol, JSReceiver
// - Call context: direct call, inside try/catch, inside catch block,
//                 nested try/catch inside catch

d8.file.execute('test/mjsunit/mjsunit.js');
d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

function createIdentityModule(sig) {
  let builder = new WasmModuleBuilder();
  builder.addFunction("f", sig)
    .addBody([kExprLocalGet, 0])
    .exportAs("f");
  return builder.instantiate();
}

function warmUpAndOptimize(fn, warmUpValues) {
  %PrepareFunctionForOptimization(fn);
  for (let v of warmUpValues) fn(v);
  %OptimizeFunctionOnNextCall(fn);
  // One more call to trigger optimization.
  fn(warmUpValues[0]);
}

////////////////////////////////////////////////////////////////////////////
// Direct calls (no try/catch) — LazyDeoptOnThrow::kYes
// Values that don't match the expected type cause eager deopt (i32/f32/f64)
// or a thrown exception (i64 BigIntToI64 with frame state).

// CHECK-LABEL: testI32Direct
(function testI32Direct() {
  print("testI32Direct");
  let instance = createIdentityModule(kSig_i_i);

  // CHECK: Considering JS-to-Wasm wrapper for Wasm function [0] f of module
  // CHECK-NEXT: - inlining wrapper
  function call(x) { return instance.exports.f(x); }
  warmUpAndOptimize(call, [1, 2, 3]);

  // Smi — fast path, stays optimized.
  assertEquals(42, call(42));

  // HeapNumber — fast path (truncates to i32).
  assertEquals(3, call(3.7));

  // BigInt — eager deopt, then interpreter throws.
  assertThrows(() => call(1n), TypeError);

  // Symbol — eager deopt, then interpreter throws.
  assertThrows(() => call(Symbol("s")), TypeError);

  // JSReceiver — eager deopt, then interpreter calls valueOf.
  let count = 0;
  assertEquals(7, call({ valueOf() { ++count; return 7; } }));
  assertEquals(1, count);
})();

// CHECK-LABEL: testI64Direct
(function testI64Direct() {
  print("testI64Direct");
  let instance = createIdentityModule(kSig_l_l);

  // CHECK: Considering JS-to-Wasm wrapper for Wasm function [0] f of module
  // CHECK-NEXT: - inlining wrapper
  function call(x) { return instance.exports.f(x); }
  warmUpAndOptimize(call, [0n, 1n, 2n]);

  // BigInt — fast path.
  assertEquals(42n, call(42n));

  // Smi — BigIntToI64 builtin throws TypeError (with frame state for deopt).
  assertThrows(() => call(42), TypeError);

  // HeapNumber — BigIntToI64 throws.
  assertThrows(() => call(3.14), TypeError);

  // String — BigIntToI64 throws SyntaxError.
  assertThrows(() => call("hello"), SyntaxError);

  // Symbol — BigIntToI64 throws TypeError.
  assertThrows(() => call(Symbol("s")), TypeError);

  // JSReceiver — ToBigInt calls valueOf, succeeds if it returns a BigInt.
  let count = 0;
  assertEquals(1n, call({ valueOf() { ++count; return 1n; } }));
  assertEquals(1, count);
})();

// CHECK-LABEL: testF32Direct
(function testF32Direct() {
  print("testF32Direct");
  let instance = createIdentityModule(kSig_f_f);

  // CHECK: Considering JS-to-Wasm wrapper for Wasm function [0] f of module
  // CHECK-NEXT: - inlining wrapper
  function call(x) { return instance.exports.f(x); }
  warmUpAndOptimize(call, [1.0, 2.0, 3.0]);

  // Smi.
  assertEquals(42, call(42));

  // HeapNumber.
  assertEqualsDelta(3.14, call(3.14), 0.01);

  // BigInt — eager deopt, then interpreter throws.
  assertThrows(() => call(1n), TypeError);

  // Symbol — eager deopt, then interpreter throws.
  assertThrows(() => call(Symbol("s")), TypeError);

  // JSReceiver — eager deopt, then interpreter calls valueOf.
  let count = 0;
  assertEquals(7, call({ valueOf() { ++count; return 7; } }));
  assertEquals(1, count);
})();

// CHECK-LABEL: testF64Direct
(function testF64Direct() {
  print("testF64Direct");
  let instance = createIdentityModule(kSig_d_d);

  // CHECK: Considering JS-to-Wasm wrapper for Wasm function [0] f of module
  // CHECK-NEXT: - inlining wrapper
  function call(x) { return instance.exports.f(x); }
  warmUpAndOptimize(call, [1.0, 2.0, 3.0]);

  // Smi.
  assertEquals(42, call(42));

  // HeapNumber.
  assertEquals(3.14, call(3.14));

  // BigInt — eager deopt, then interpreter throws.
  assertThrows(() => call(1n), TypeError);

  // Symbol — eager deopt, then interpreter throws.
  assertThrows(() => call(Symbol("s")), TypeError);

  // JSReceiver — eager deopt, interpreter calls valueOf.
  let count = 0;
  assertEquals(7, call({ valueOf() { ++count; return 7; } }));
  assertEquals(1, count);
})();

///////////////////////////////////////////////////////////////////
// Inside try/catch — LazyDeoptOnThrow::kNo (catch handler present)
// Same deopt behavior but exceptions are caught.

// CHECK-LABEL: testI32TryCatch
(function testI32TryCatch() {
  print("testI32TryCatch");
  let instance = createIdentityModule(kSig_i_i);

  // CHECK: Considering JS-to-Wasm wrapper for Wasm function [0] f of module
  // CHECK-NEXT: - inlining wrapper
  function call(x) {
    try {
        return instance.exports.f(x);
    }
    catch (e) {
        return e;
    }
  }
  warmUpAndOptimize(call, [1, 2, 3]);

  assertEquals(42, call(42));
  assertEquals(3, call(3.7));

  let e1 = call(1n);
  assertTrue(e1 instanceof TypeError);

  let e2 = call(Symbol("s"));
  assertTrue(e2 instanceof TypeError);

  let count = 0;
  assertEquals(7, call({ valueOf() { ++count; return 7; } }));
  assertEquals(1, count);
})();

// CHECK-LABEL: testI64TryCatch
(function testI64TryCatch() {
  print("testI64TryCatch");
  let instance = createIdentityModule(kSig_l_l);

  // CHECK: Considering JS-to-Wasm wrapper for Wasm function [0] f of module
  // CHECK-NEXT: - inlining wrapper
  function call(x) {
    try {
        return instance.exports.f(x);
    }
    catch (e) {
        return e;
    }
  }
  warmUpAndOptimize(call, [0n, 1n, 2n]);

  assertEquals(42n, call(42n));

  let e1 = call(42);
  assertTrue(e1 instanceof TypeError);

  let e2 = call("hello");
  assertTrue(e2 instanceof SyntaxError);

  let e3 = call(Symbol("s"));
  assertTrue(e3 instanceof TypeError);

  // JSReceiver — ToBigInt calls valueOf, succeeds.
  let count = 0;
  assertEquals(1n, call({ valueOf() { ++count; return 1n; } }));
  assertEquals(1, count);
})();

// CHECK-LABEL: testF32TryCatch
(function testF32TryCatch() {
  print("testF32TryCatch");
  let instance = createIdentityModule(kSig_f_f);

  // CHECK: Considering JS-to-Wasm wrapper for Wasm function [0] f of module
  // CHECK-NEXT: - inlining wrapper
  function call(x) {
    try {
        return instance.exports.f(x);
    }
    catch (e) {
        return e;
    }
  }
  warmUpAndOptimize(call, [1.0, 2.0, 3.0]);

  assertEquals(42, call(42));
  assertEqualsDelta(3.14, call(3.14), 0.01);

  let e1 = call(1n);
  assertTrue(e1 instanceof TypeError);

  let e2 = call(Symbol("s"));
  assertTrue(e2 instanceof TypeError);

  let count = 0;
  assertEquals(7, call({ valueOf() { ++count; return 7; } }));
  assertEquals(1, count);
})();

// CHECK-LABEL: testF64TryCatch
(function testF64TryCatch() {
  print("testF64TryCatch");
  let instance = createIdentityModule(kSig_d_d);

  // CHECK: Considering JS-to-Wasm wrapper for Wasm function [0] f of module
  // CHECK-NEXT: - inlining wrapper
  function call(x) {
    try {
        return instance.exports.f(x);
    }
    catch (e) {
        return e;
    }
  }
  warmUpAndOptimize(call, [1.0, 2.0, 3.0]);

  assertEquals(42, call(42));
  assertEquals(3.14, call(3.14));

  let e1 = call(1n);
  assertTrue(e1 instanceof TypeError);

  let e2 = call(Symbol("s"));
  assertTrue(e2 instanceof TypeError);

  let count = 0;
  assertEquals(7, call({ valueOf() { ++count; return 7; } }));
  assertEquals(1, count);
})();

/////////////////////////////////////////////////////////////////////////////
// Called from inside a catch block — the Wasm call itself is still a direct
// call (no enclosing try for the Wasm call), but it's reachable from a catch
// block.

// CHECK-LABEL: testI32FromCatchBlock
(function testI32FromCatchBlock() {
  print("testI32FromCatchBlock");
  let instance = createIdentityModule(kSig_i_i);

  // CHECK: Considering JS-to-Wasm wrapper for Wasm function [0] f of module
  // CHECK-NEXT: - inlining wrapper
  function call(x) {
    try {
      throw "trigger";
    } catch (e) {
      // Wasm call happens inside the catch block.
      return instance.exports.f(x);
    }
  }
  warmUpAndOptimize(call, [1, 2, 3]);

  assertEquals(42, call(42));
  let count = 0;
  assertEquals(7, call({ valueOf() { ++count; return 7; } }));
  assertEquals(1, count);
  assertThrows(() => call(1n), TypeError);
})();

// CHECK-LABEL: testI64FromCatchBlock
(function testI64FromCatchBlock() {
  print("testI64FromCatchBlock");
  let instance = createIdentityModule(kSig_l_l);

  // CHECK: Considering JS-to-Wasm wrapper for Wasm function [0] f of module
  // CHECK-NEXT: - inlining wrapper
  function call(x) {
    try {
      throw "trigger";
    } catch (e) {
      return instance.exports.f(x);
    }
  }
  warmUpAndOptimize(call, [0n, 1n, 2n]);

  assertEquals(42n, call(42n));
  assertThrows(() => call(42), TypeError);
  assertThrows(() => call("hello"), SyntaxError);
})();

// CHECK-LABEL: testF64FromCatchBlock
(function testF64FromCatchBlock() {
  print("testF64FromCatchBlock");
  let instance = createIdentityModule(kSig_d_d);

  // CHECK: Considering JS-to-Wasm wrapper for Wasm function [0] f of module
  // CHECK-NEXT: - inlining wrapper
  function call(x) {
    try {
      throw "trigger";
    } catch (e) {
      return instance.exports.f(x);
    }
  }
  warmUpAndOptimize(call, [1.0, 2.0, 3.0]);

  assertEquals(42, call(42));
  assertEquals(3.14, call(3.14));
  let count = 0;
  assertEquals(7, call({ valueOf() { ++count; return 7; } }));
  assertEquals(1, count);
  assertThrows(() => call(1n), TypeError);
})();

///////////////////////////////////////////////////////////////////////////
// Nested try/catch with Wasm call inside catch — the Wasm call has its own
// try/catch wrapper.

// CHECK-LABEL: testI64CatchInsideCatch
(function testI64CatchInsideCatch() {
  print("testI64CatchInsideCatch");
  let instance = createIdentityModule(kSig_l_l);

  // CHECK: Considering JS-to-Wasm wrapper for Wasm function [0] f of module
  // CHECK-NEXT: - inlining wrapper
  function call(x) {
    try {
      throw "outer";
    } catch (outerE) {
      try {
        return instance.exports.f(x);
      } catch (innerE) {
        return innerE;
      }
    }
  }
  warmUpAndOptimize(call, [0n, 1n, 2n]);

  assertEquals(42n, call(42n));

  let e1 = call(42);
  assertTrue(e1 instanceof TypeError);

  let e2 = call("bad");
  assertTrue(e2 instanceof SyntaxError);
})();
