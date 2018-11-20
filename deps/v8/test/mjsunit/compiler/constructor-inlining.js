// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --stress-inline

var counter = 0;
var deopt_at = -1;

class Base {
  constructor(use, x){
    if (deopt_at-- == 0) {
        %_DeoptimizeNow();
        %DeoptimizeFunction(testConstructorInlining);
    }
    counter++;
    this.x = x;
    if (use) {
      return x;
    }
  }
}

class Derived extends Base {
  constructor(use, x, y, deopt = false) {
    super(use, x);
    counter++;
    if (deopt_at-- == 0) %_DeoptimizeNow();
    this.y = y;
    if (use) {
      return y;
    }
  }
}

var DerivedDeoptCreate =  new Proxy(Derived, {
  get: function(target, name) {
    if (name=='prototype') {
      counter++;
      if (deopt_at-- == 0) %DeoptimizeFunction(Derived);
    }
    return target[name];
  }
});

function Constr(use, x){
  counter++;
  if (deopt_at-- == 0) %_DeoptimizeNow();
  this.x = x;
  if (use) {
    return x;
  }
}


var a = {};
var b = {};

function testConstructorInlining(){
  assertEquals(a, new Constr(true, a));
  assertEquals(7, new Constr(false, 7).x);
  assertEquals(5, new Constr(true, 5).x);

  assertEquals(a, new Base(true, a));
  assertEquals(7, new Base(false, 7).x);
  assertEquals(5, new Base(true, 5).x);

  assertEquals(b, new Derived(true, a, b));
  assertEquals(a, new Derived(true, a, undefined));
  assertEquals(5, new Derived(false, 5, 7).x);
  assertEquals(7, new Derived(false, 5, 7).y);
  try {
    new Derived(true, a, 7)
    assertTrue(false);
  } catch (e) {
    if (!(e instanceof TypeError)) throw e;
  }
  assertEquals(a, new Derived(true, 5, a));

  %OptimizeFunctionOnNextCall(Derived);
  assertEquals(b, new DerivedDeoptCreate(true, a, b));
  %OptimizeFunctionOnNextCall(Derived);
  assertEquals(a, new DerivedDeoptCreate(true, a, undefined));
  %OptimizeFunctionOnNextCall(Derived);
  assertEquals(5, new DerivedDeoptCreate(false, 5, 7).x);
  %OptimizeFunctionOnNextCall(Derived);
  assertEquals(7, new DerivedDeoptCreate(false, 5, 7).y);
}

testConstructorInlining();
%OptimizeFunctionOnNextCall(testConstructorInlining);
testConstructorInlining();

var last = undefined;
for(var i = 0; deopt_at < 0; ++i) {
  deopt_at = i;
  counter = 0;
  %OptimizeFunctionOnNextCall(testConstructorInlining);
  testConstructorInlining();
  if (last !== undefined) {
    assertEquals(counter, last)
  }
  last = counter;
}
