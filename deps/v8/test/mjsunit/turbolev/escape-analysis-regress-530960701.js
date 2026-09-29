// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-assert --turbofan
// Flags: --turbolev --turbolev-escape-analysis

function get_obj() {
  return { x : 42 };
}

function foo(b) {
  // Getting an object from a non-eager-inlined function. This means that when
  // building the initial graph for `foo`, `obj` will not be in the
  // VirtualObject lists of the DeoptFrames (since when the DeoptFrames are
  // created, `obj` is just an opaque CallKnownJSFunction).
  let obj = get_obj();

  for (let i = 0; i < 10; i++) {
    // A loop so that we get a JumpLoop at this end, which holds an eager
    // DeoptFrame despite not having can_eager_deopt properties.
  }

  if (b) {
    // Keeping `outer_obj` alive, but not without blocking eliding: we won't
    // have feedback for this path so we'll get an unconditional deopt.
    return obj.x;
  }
}

%PrepareFunctionForOptimization(get_obj);
%PrepareFunctionForOptimization(foo);
foo(false);
foo(false);

%OptimizeFunctionOnNextCall(foo);
foo(false);
assertOptimized(foo);

// Triggering deopt, checking result.
assertEquals(42, foo(true));
assertUnoptimized(foo);
