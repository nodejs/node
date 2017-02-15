// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function F() {}
var f = new F

function foo(x) { return x instanceof F };
%OptimizeFunctionOnNextCall(foo);
assertFalse(foo(1));

var proto = Object.getPrototypeOf(F);
Object.setPrototypeOf(F, null);
F[Symbol.hasInstance] = function(v) { return true };
Object.setPrototypeOf(F, proto);

assertTrue(foo(1));
