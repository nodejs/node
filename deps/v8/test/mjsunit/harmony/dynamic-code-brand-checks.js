// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

// Tests for Dynamic Code Brand Checks in CreateDynamicFunction
// (builtins-function.cc). These tests verify the HostGetCodeForEval-style
// fallback-to-ToString behavior when no embedder callback is installed,
// and the codegen_allowed rejection path via %DisallowCodegenFromStrings.
// The callback's HostGetCodeForEval-style code-substitution path is
// covered in more detail via cctest, since installing a callback that
// distinguishes between argument shapes isn't possible from pure JS.

(function TestNormalFunctionConstructorStillWorks() {
  const f = new Function("a", "b", "return a + b;");
  assertEquals(3, f(1, 2));

  const g = new (function*(){}).constructor("a", "yield a;");
  assertEquals(5, g(5).next().value);

  const af = new (async function(){}).constructor("a", "return a;");
  assertInstanceof(af(7), Promise);
})();

// --- Zero-argument Function constructor (regression test) ---
//
// `new Function()` passes zero arguments, so there is no body argument
// at all. This must not be confused with the receiver (the constructor
// itself), which sits at BuiltinArguments index 0.
(function TestZeroArgumentFunctionConstructorHasEmptyBody() {
  const f = new Function();
  assertEquals(undefined, f());
  assertTrue(f.toString().includes("anonymous"));
})();

// Fallback to ToString when no embedder callback is installed
(function TestObjectArgumentFallsBackToToString() {
  // With no embedder callback installed, HostGetCodeForEval has no
  // effect, so object arguments must fall back to ToString().
  let toStringCallCount = 0;
  const bodyArg = {
    toString() {
      toStringCallCount++;
      return "return a;";
    }
  };

  const f = new Function("a", bodyArg);
  assertEquals(1, toStringCallCount);
  assertEquals(5, f(5));
})();

(function TestSymbolToPrimitiveUsedForObjectArgument() {
  let calls = 0;
  const paramArg = {
    [Symbol.toPrimitive](hint) {
      calls++;
      return "a";
    }
  };

  const f = new Function(paramArg, "return a;");
  assertEquals(1, calls);
  assertEquals(9, f(9));
})();

// Rejection when the embedder callback disallows codegen
(function TestCallbackRejectionThrowsEvalError() {
  // %DisallowCodegenFromStrings installs a callback that always sets
  // codegen_allowed = false. Only object (JSReceiver) arguments reach
  // the callback; plain strings never do.
  %DisallowCodegenFromStrings(true);
  try {
    assertThrows(() => {
      new Function({ toString() { return "a"; } }, "return 1;");
    }, EvalError);
  } finally {
    %DisallowCodegenFromStrings(false);
  }
})();
