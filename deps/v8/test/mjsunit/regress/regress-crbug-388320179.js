// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// The heap broker caches a JSFunction's prototype, which may be another
// JSFunction. Caching a chain of them must not recurse, or compiling any
// function that refers to the head of the chain overflows the stack.

// Links alternate between the two ways the broker reaches the next function:
// directly through the prototype slot, and through the prototype of an initial
// map. The chain has to outlast the C++ stack; it overflowed at ~22000 links.
const kChainLength = 80000;

let f = function() {};
for (let i = 0; i < kChainLength; i++) {
  const g = function() {};
  g.prototype = f;
  if (i % 2) new g();  // Materializes an initial map with prototype {f}.
  f = g;
}

const head = f;
function call_head() { return head(); }

%PrepareFunctionForOptimization(call_head);
call_head();
%OptimizeFunctionOnNextCall(call_head);
assertEquals(undefined, call_head());
