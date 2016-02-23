// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-proxies

function bar() {}
bar({ a: Proxy.create({}) });
function foo(x) { x.a.b == ""; }
var x = {a: {b: "" }};
foo(x);
foo(x);
%OptimizeFunctionOnNextCall(foo);
foo(x);
