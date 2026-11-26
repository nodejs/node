// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

function tryCall(f) {
  try {
    return f();
  } catch (e) {
  }
}
%PrepareFunctionForOptimization(tryCall);
%DisableEagerInlining(tryCall);
// Exercise the catch path so Maglev emits a return merge Phi instead of
// deoptimizing on exception.
tryCall(() => { throw 0; });

function makeArray() {
  return [1, 2, 3];
}
%PrepareFunctionForOptimization(makeArray);
%DisableEagerInlining(makeArray);

function readLength(x) {
  // Keep bytecode larger than tryCall/makeArray so MaglevInliner inlines
  // tryCall and makeArray first, reducing `arr` to a single-input Phi wrapping
  // an InlinedAllocation before inlining this polymorphic load.
  const a = x.length;
  const b = x.length;
  const c = x.length;
  const d = x.length;
  return a + b + c + d;
}
%PrepareFunctionForOptimization(readLength);
%DisableEagerInlining(readLength);

function foo() {
  const arr = tryCall(makeArray);
  readLength(1);
  return readLength(arr);
}
%PrepareFunctionForOptimization(foo);

assertEquals(12, foo());
assertEquals(12, foo());
%OptimizeFunctionOnNextCall(foo);
assertEquals(12, foo());
