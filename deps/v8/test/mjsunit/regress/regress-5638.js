// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

class MyErrorA {}

class MyErrorB {}

class A {}

class B extends A {
  constructor() {
    try {
      super();
    } catch (e) {
      throw new MyErrorB();
    }
  }
}

var thrower = new Proxy(A, {
  get(target, property, receiver) {
    if (property === 'prototype') throw new MyErrorA();
  }
});

assertThrows(() => Reflect.construct(B, [], thrower), MyErrorB);
assertThrows(() => Reflect.construct(B, [], thrower), MyErrorB);
%OptimizeFunctionOnNextCall(B);
assertThrows(() => Reflect.construct(B, [], thrower), MyErrorB);
