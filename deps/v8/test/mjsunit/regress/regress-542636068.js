// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

// Maglev unwraps bound functions recursively when reducing instanceof. A long
// bound_target_function chain must not overflow the compiler's stack.
// Turbolev is requested explicitly because the classic Turbofan pipeline hits
// an unrelated deep recursion in late load elimination on the same input.
function mkBound(depth) {
  function C() {}
  let g = C;
  for (let i = 0; i < depth; ++i) g = g.bind();
  return g;
}

// Constant callable.
const kDeep = mkBound(50000);
function constant_callable(o) {
  try {
    return o instanceof kDeep;
  } catch (e) {
    return 'threw';
  }
}
%PrepareFunctionForOptimization(constant_callable);
constant_callable({});
%OptimizeFunctionOnNextCall(constant_callable);
constant_callable({});

// Callable from instanceof feedback.
function feedback_callable(o, callable) {
  try {
    return o instanceof callable;
  } catch (e) {
    return 'threw';
  }
}
%PrepareFunctionForOptimization(feedback_callable);
feedback_callable({}, kDeep);
%OptimizeFunctionOnNextCall(feedback_callable);
feedback_callable({}, kDeep);

// Shallow bound chains still reduce correctly.
function C() {}
const bound = C.bind().bind();
function shallow(o) {
  return o instanceof bound;
}
%PrepareFunctionForOptimization(shallow);
assertTrue(shallow(new C()));
assertFalse(shallow({}));
%OptimizeFunctionOnNextCall(shallow);
assertTrue(shallow(new C()));
assertFalse(shallow({}));
