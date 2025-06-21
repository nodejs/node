// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

class Foo extends Array {
  constructor() {
    super();
  }
};

// Invalidate the array iterator, for FindNonDefaultConstructorOrConstruct
// reduction.
Array.prototype[Symbol.iterator] = function () {};

%PrepareFunctionForOptimization(Foo);
new Foo();
new Foo();
%OptimizeMaglevOnNextCall(Foo);
new Foo();

// This should invalidate the Foo code dependency.
Foo.__proto__ = [1];

// Should throw an exception because the super constructor is no longer a
// constructor (but an array).
assertThrows(()=>new Foo());
