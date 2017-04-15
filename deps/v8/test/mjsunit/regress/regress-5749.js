// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(x) {
    (x ** 1) === '';
}
f();
f();
f();
%OptimizeFunctionOnNextCall(f);
f();

function g(x) {
    '' === (x ** 1);
}
g();
g();
g();
%OptimizeFunctionOnNextCall(g);
g();
