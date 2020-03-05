// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --no-always-opt

function f(x) {
  %TurbofanStaticAssert(x.foo === 42);
  return %IsBeingInterpreted();
}

function main(b, ret) {
  const x = new Object();
  const y = x;
  if (b) {
    return ret;
  } else {
    x.foo = 42;
    return f(y);
  }
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
