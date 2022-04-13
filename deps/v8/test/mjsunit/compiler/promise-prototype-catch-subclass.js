// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let custom_then_called = false;

function foo(p) {
  custom_then_called = false;
  p.catch(x => x);
  return custom_then_called;
}

class MyPromise extends Promise {
  then(onFulfilled, onRejected) {
    custom_then_called = true;
    return super.then(onFulfilled, onRejected);
  }
}

const a = MyPromise.resolve(1);

%PrepareFunctionForOptimization(foo);
assertTrue(foo(a));
assertTrue(foo(a));
%OptimizeFunctionOnNextCall(foo);
assertTrue(foo(a));
