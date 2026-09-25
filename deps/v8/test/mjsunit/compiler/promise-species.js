// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

class Foo extends Promise {};

function f() {
  return new Promise(r => 88).then(x => 88);
}

%PrepareFunctionForOptimization(f);

let y;

y = f();
assertFalse(y instanceof Foo);

y = f();
assertFalse(y instanceof Foo);

%OptimizeFunctionOnNextCall(f);

y = f();
assertFalse(y instanceof Foo);

assertTrue(Reflect.defineProperty(Promise, Symbol.species, {value: Foo}));

y = f();
assertInstanceof(y, Foo);
