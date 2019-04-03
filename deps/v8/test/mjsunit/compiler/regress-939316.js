// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(arg) {
  const o = Reflect.construct(Object, arguments, Proxy);
  o.foo = arg;
}

function g(i) {
  f(i);
}

g(0);
g(1);
%OptimizeFunctionOnNextCall(g);
g(2);
