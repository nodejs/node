// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --modify-field-representation-inplace
// Flags: --no-always-opt --opt

// Test that code embedding accesses to a Smi field gets properly
// deoptimized if s->t field representation changes are done in-place.
(function() {
  function O(x) { this.x = x; }

  function foo(o) { return o.x; }
  %PrepareFunctionForOptimization(foo);
  // We need to keep an instance around to make the GC stress testing work.
  const o1 = new O(1);
  foo(o1);
  foo(new O(2));
  %OptimizeFunctionOnNextCall(foo);
  foo(new O(3));
  assertOptimized(foo);

  new O(null);
  assertUnoptimized(foo);
})();

// Test that code embedding assignments to a Smi field gets properly
// deoptimized if s->t field representation changes are done in-place.
(function() {
  function O(x) { this.x = x; }

  function foo(o) { o.x = 0; }
  %PrepareFunctionForOptimization(foo);
  // We need to keep an instance around to make the GC stress testing work.
  const o1 = new O(1);
  foo(o1);
  foo(new O(2));
  %OptimizeFunctionOnNextCall(foo);
  foo(new O(3));
  assertOptimized(foo);

  new O(null);
  assertUnoptimized(foo);
})();

// Test that code embedding accesses to a HeapObject field gets properly
// deoptimized if h->t field representation changes are done in-place.
(function() {
  function O(x) { this.x = x; }

  function foo(o) { return o.x; }
  %PrepareFunctionForOptimization(foo);
  // We need to keep an instance around to make the GC stress testing work.
  const onull = new O(null);
  foo(onull);
  foo(new O("Hello"));
  %OptimizeFunctionOnNextCall(foo);
  foo(new O({}));
  assertOptimized(foo);

  new O(1);
  assertUnoptimized(foo);
})();

// Test that code embedding assignments to a Smi field gets properly
// deoptimized if s->t field representation changes are done in-place.
(function() {
  function O(x) { this.x = x; }

  function foo(o) { o.x = true; }
  %PrepareFunctionForOptimization(foo);
  // We need to keep an instance around to make the GC stress testing work.
  const onull = new O(null);
  foo(onull);
  foo(new O("Hello"));
  %OptimizeFunctionOnNextCall(foo);
  foo(new O({}));
  assertOptimized(foo);

  new O(1);
  assertUnoptimized(foo);
})();
