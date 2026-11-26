// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// The instance members initializer call in class constructors is emitted
// without an undefined check; super() from arrows and eval keeps it.

class Vector {
  x = 0;
  y = 0;
  z = 0;
  constructor(x, y, z) {
    this.x = x;
    this.y = y;
    this.z = z;
  }
  static add(a, b) {
    return new Vector(a.x + b.x, a.y + b.y, a.z + b.z);
  }
}

class Derived extends Vector {
  w = 4;
  constructor() {
    super(1, 2, 3);
  }
}

class DerivedDefault extends Vector {
  w = 5;
}

class DerivedArrow extends Vector {
  w = 6;
  constructor() {
    const f = () => super(1, 2, 3);
    f();
  }
}

class DerivedEval extends Vector {
  w = 7;
  constructor() {
    eval('super(1, 2, 3)');
  }
}

class NoFieldsBase {}
class DerivedArrowNoFields extends NoFieldsBase {
  constructor() {
    const f = () => super();
    f();
  }
}
class DerivedEvalNoFields extends NoFieldsBase {
  constructor() {
    eval('super()');
  }
}

function mid(a, b) {
  return Vector.add(a, b);
}

function outer(a, b) {
  return mid(a, b).x;
}

function check() {
  const v = new Vector(1.5, 2.5, 3.5);
  assertEquals(3, outer(v, v));
  assertEquals([1, 2, 3, 4], Object.values(new Derived()));
  assertEquals([1, 2, 3, 5], Object.values(new DerivedDefault(1, 2, 3)));
  assertEquals([1, 2, 3, 6], Object.values(new DerivedArrow()));
  assertEquals([1, 2, 3, 7], Object.values(new DerivedEval()));
  assertEquals([], Object.keys(new DerivedArrowNoFields()));
  assertEquals([], Object.keys(new DerivedEvalNoFields()));
}

%PrepareFunctionForOptimization(Vector);
%PrepareFunctionForOptimization(%GetInitializerFunction(Vector));
%PrepareFunctionForOptimization(outer);
%PrepareFunctionForOptimization(mid);
%PrepareFunctionForOptimization(Vector.add);
check();
check();
%OptimizeMaglevOnNextCall(outer);
check();
assertOptimized(outer);

// A field initializer that throws propagates out of the constructor.
class Throwing {
  a = (() => {
    throw new Error('boom');
  })();
}
assertThrows(() => new Throwing(), Error, 'boom');
