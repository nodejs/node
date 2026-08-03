// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function C(a, b) {
  this.a = a;
  this.b = b;
}

var c = new C(42, 4.2);
var cc = new C(4.2, 42);

function foo() { return cc.a }

%PrepareFunctionForOptimization(foo);
assertEquals(4.2, foo());

// Transition representation of 'a' from Double to Tagged.
Object.defineProperty(c, 'a', { value: undefined });

%OptimizeFunctionOnNextCall(foo);
assertEquals(4.2, foo());
