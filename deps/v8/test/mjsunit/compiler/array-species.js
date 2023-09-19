// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function Foo() {}

function f() {
  return [42].map(_ => 88);
}

let y;

%PrepareFunctionForOptimization(f);

y = f();
assertFalse(y instanceof Foo);
assertInstanceof(y, Array);

y = f();
assertFalse(y instanceof Foo);
assertInstanceof(y, Array);

%OptimizeFunctionOnNextCall(f);

y = f();
assertFalse(y instanceof Foo);
assertInstanceof(y, Array);

assertTrue(Reflect.defineProperty(Array, Symbol.species, {value: Foo}));

y = f();
assertInstanceof(y, Foo);
assertFalse(y instanceof Array);
