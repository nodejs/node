// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboprop --turbo-dynamic-map-checks

function bar(obj) {
  return Object.getPrototypeOf(obj);
}

function foo(a, b) {
  try {
    a.a;
  } catch (e) {}
  try {
    b[bar()] = 1;
  } catch (e) {}
}

var arg = {
  a: 10,
};

%PrepareFunctionForOptimization(foo);
foo(arg);
%OptimizeFunctionOnNextCall(foo);
foo();
