// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --omit-default-ctors --allow-natives-syntax

function f() {}

Function.prototype.__proto__ = new Proxy(f, {});

class C0 {}
class C1 extends C0 {}

%PrepareFunctionForOptimization(C1);
new C1();
%OptimizeFunctionOnNextCall(C1);
new C1();
