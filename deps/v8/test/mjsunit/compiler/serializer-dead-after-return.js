// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --no-always-opt

function f(x) {
  if (!%IsDictPropertyConstTrackingEnabled()) {
    // TODO(v8:11457) If v8_dict_property_const_tracking is enabled, then the
    // prototype of |x| in |main| is a dictionary mode object, and we cannot
    // inline the storing of x.foo, yet.
    %TurbofanStaticAssert(x.foo === 42);
  }
  return %IsBeingInterpreted();
}

function main(b, ret) {
  const x = new Object();
  const y = x;
  if (b) return ret;

  x.foo = 42;
  out = x;  // Prevent x's new map from dying too early.
  return f(y);
}


%PrepareFunctionForOptimization(f);
%PrepareFunctionForOptimization(main);

f({a: 1});
f({b: 1});
f({c: 1});
f({d: 1});

assertTrue(main(true, true));
assertTrue(main(true, true));
assertTrue(main(false, true));
assertTrue(main(false, true));
%OptimizeFunctionOnNextCall(main);
assertFalse(main(false));
