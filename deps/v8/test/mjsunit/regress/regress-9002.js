// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

function f() {
  return 42;
}

function g() {
  return 52;
}

%NeverOptimizeFunction(f);

function foo(cond) {
  let func;
  if (cond) {
    func = f;
  } else {
    func = g;
  }
  func();
}

%PrepareFunctionForOptimization(foo);
foo(true);
foo(false);
%OptimizeFunctionOnNextCall(foo);
foo(true);
foo(false);

// Just a sanitary check, we have a DCHECK in js-inlining.cc to make sure
// f is not inlined into foo.
assertUnoptimized(f);
