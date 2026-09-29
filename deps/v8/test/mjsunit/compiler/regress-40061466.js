// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --expose-gc --no-turbo-inlining --verify-heap

function set_inner_foo(o) {
  o.inner.foo = {};
}

function makeObj() {
  return {
    inner: {
      ['foo']: 0
    }
  };
}

function round() {
  var o = makeObj();
  set_inner_foo(o);
  %OptimizeFunctionOnNextCall(set_inner_foo);
  set_inner_foo(o);
  return o;
}

%PrepareFunctionForOptimization(makeObj);
%PrepareFunctionForOptimization(set_inner_foo);
round();
gc();
round();
gc();

%OptimizeFunctionOnNextCall(makeObj);
round();
