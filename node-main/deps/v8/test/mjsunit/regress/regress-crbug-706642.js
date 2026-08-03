// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

class A extends Object {
  constructor(arg) {
    super();
    superclass_counter++;
    if (superclass_counter === 3) {
      return 1;
    }
  }
}

class B extends A {
  constructor() {
    let x = super(123);
    return x.a;
  }
}

var superclass_counter = 0;
var observer = new Proxy(A, {
  get(target, property, receiver) {
    if (property === 'prototype') {
      %DeoptimizeFunction(B);
    }
    return Reflect.get(target, property, receiver);
  }
});

%PrepareFunctionForOptimization(B);
Reflect.construct(B, [], observer);
Reflect.construct(B, [], observer);
%OptimizeFunctionOnNextCall(B);
assertThrows(() => Reflect.construct(B, [], observer), TypeError);
