// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

// The graph builder stops unwrapping bound functions at kMaxBoundFunctionDepth
// and emits an OrdinaryHasInstance builtin call with the remaining callable as
// a constant. The graph optimizer must not resume unwrapping that chain: it
// would emit map checks, and the CallBuiltin it is visiting has no eager deopt
// info to hang them off.

function RealCtor() {}

// One level deeper than kMaxBoundFunctionDepth. Built before RealCtor loses
// Function.prototype, which is where bind lives.
let bound_chain = RealCtor;
for (let i = 0; i < 6; ++i) bound_chain = bound_chain.bind(null);

// Without Function.prototype in the chain, @@hasInstance is not found, which is
// the reduction path that emits map checks.
Object.setPrototypeOf(RealCtor, null);
RealCtor.x = 1;

// Transition a sibling off RealCtor's map to make that map unstable, so the
// reduction emits a CheckMaps instead of taking a stable map dependency.
function DummyCtor() {}
Object.setPrototypeOf(DummyCtor, null);
DummyCtor.x = 1;
DummyCtor.y = 2;

function test(o) {
  return o instanceof bound_chain;
}

%PrepareFunctionForOptimization(test);
assertFalse(test({}));
assertTrue(test(new RealCtor()));
%OptimizeFunctionOnNextCall(test);
assertFalse(test({}));
assertTrue(test(new RealCtor()));
