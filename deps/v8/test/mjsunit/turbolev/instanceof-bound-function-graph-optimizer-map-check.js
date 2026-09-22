// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

// Reducing `instanceof` on a bound function whose callable isn't a constant
// leaves a CallBuiltin(OrdinaryHasInstance) behind. The graph optimizer later
// knows the callable, unwraps the bound target and emits a CheckMaps for it.
// CheckMaps can eager deopt, so this only works if an eager deopt frame is
// available, which CallBuiltin (unlike TestInstanceOf) does not have.

function Foo() {}
function Decoy() {}

// With a null prototype, looking up Symbol.hasInstance on Foo is a "not found"
// access, which is the path that checks the callable's map.
Object.setPrototypeOf(Foo, null);

// Decoy shares Foo's map and then transitions away from it, which makes Foo's
// map unstable. The reduction therefore has to emit an actual CheckMaps rather
// than just registering a stable map dependency.
Object.setPrototypeOf(Decoy, null);
Decoy.x = 1;

const Bound = Function.prototype.bind.call(Foo, null);

function test(obj, n) {
  let result;
  // `n` keeps the trip count opaque, so the callable stays a loop phi while the
  // graph builder runs and it has to fall back to a CallBuiltin. The feedback
  // stays monomorphic on Bound though, since callers only ever pass n == 1.
  let c = Bound;
  for (let i = 0; i < n; i++) {
    result = obj instanceof c;
    c = Array;
  }
  return result;
}

%PrepareFunctionForOptimization(test);
assertFalse(test({}, 1));

%OptimizeFunctionOnNextCall(test);
assertFalse(test({}, 1));
assertTrue(test(Object.create(Foo.prototype), 1));
