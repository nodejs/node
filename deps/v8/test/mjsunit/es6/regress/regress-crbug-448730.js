// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function bar() {}
bar({ a: new Proxy({}, {}) });
function foo(x) { x.a.b == ""; }
var x = {a: {b: "" }};
%PrepareFunctionForOptimization(foo);
foo(x);
foo(x);
%OptimizeFunctionOnNextCall(foo);
foo(x);
