// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --mark-shared-functions-for-tier-up  --allow-natives-syntax --expose-gc

function foo() {
  function bar() {
  }
  return bar;
}

// Mark bar's shared function info for tier-up
// (but don't optimize).
var bar = foo();
%OptimizeFunctionOnNextCall(bar);

// Avoid flushing foo (and thereby making bar's shared function info
// dead) by marking it to be optimized.
%OptimizeFunctionOnNextCall(foo);

// Throw away the JSFunction we have for bar and GC until its code has
// been flushed.
bar = null;
for (var i = 0; i < 6; i++) {
  gc();
}

// Now create a new JSFunction from bar's shared function info and call it,
// we should not optimize without recompiling the baseline code.
foo()();
