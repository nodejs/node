// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --sparkplug --sparkplug-plus --turbofan
// Flags: --no-always-sparkplug

// Embedded feedback in the BytecodeArray must only ever widen, even when a
// Sparkplug+ typed stub misses after the interpreter has already widened the
// feedback byte behind its back.

function f(a, b, cb) {
  cb();
  return a + b;
}
function noop() {}
function compileBaseline() { %CompileBaseline(f); }

%PrepareFunctionForOptimization(f);

// Interpreter: byte = SignedSmall.
for (let i = 0; i < 10; i++) f(1, 2, noop);

// Compile baseline from inside a live interpreted frame of f, so baseline
// picks the SignedSmall stub. The interpreted frame then widens the byte to
// Any with a string add.
f("a", "b", compileBaseline);

// Baseline: the SignedSmall stub misses on HeapNumbers. This must merge into
// the existing Any feedback rather than overwrite it with Number.
f(1.5, 2.5, noop);

%OptimizeFunctionOnNextCall(f);
f(1.5, 2.5, noop);
assertOptimized(f);

// With Any feedback the optimized code uses a generic add and must not deopt.
assertEquals("ab", f("a", "b", noop));
assertOptimized(f);
