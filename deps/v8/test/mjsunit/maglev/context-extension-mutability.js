// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev
// Flags: --optimize-on-next-call-optimizes-to-maglev

globalThis.x = "global";

// Test 1: Sloppy eval extending the outer context after `inner` has been
// optimized with function context specialization. The extension slot must not
// be constant-folded to `undefined`.
(function TestExtensionMutatedBetweenCalls() {
  function outer(cb) {
    if (!cb) {
      eval('var x = "init";');
      return;
    }
    const inner = function () {
      return x;
    };
    cb(inner, false);
    eval('var x = "shadowed";');
    cb(inner, true);
  }

  // Invalidate the EmptyContextExtension dependency on outer's ScopeInfo.
  outer(null);

  outer((inner, after_eval) => {
    if (!after_eval) {
      %PrepareFunctionForOptimization(inner);
      assertEquals("global", inner());
      %OptimizeFunctionOnNextCall(inner);
      assertEquals("global", inner());
    } else {
      assertEquals("shadowed", inner());
    }
  });
})();

// Test 2: Sloppy eval extending the outer context in the middle of an
// optimized function (via generator resume) between two dynamic global
// lookups. The extension slot load before the call must not be cached as an
// immutable constant across the call.
(function TestExtensionMutatedDuringCall() {
  let gen;
  function advance(resume) {
    if (resume) gen.next();
  }
  %NeverOptimizeFunction(advance);

  function* outer(init_eval) {
    if (init_eval) {
      eval('var x = "init";');
      return;
    }
    const inner = function (resume) {
      const before = x;
      advance(resume);
      const after = x;
      return [before, after];
    };
    yield inner;
    eval('var x = "shadowed_mid_call";');
  }

  outer(true).next();

  gen = outer(false);
  const inner = gen.next().value;
  %PrepareFunctionForOptimization(inner);
  assertEquals(["global", "global"], inner(false));
  %OptimizeFunctionOnNextCall(inner);
  assertEquals(["global", "global"], inner(false));

  assertEquals(["global", "shadowed_mid_call"], inner(true));
})();
