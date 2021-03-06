// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function TestDefaultConstructorNoCrash() {
  // Regression test for https://code.google.com/p/v8/issues/detail?id=3661
  class C {}
  assertThrows(function () {C();}, TypeError);
  assertThrows(function () {C(1);}, TypeError);
  assertTrue(new C() instanceof C);
  assertTrue(new C(1) instanceof C);
})();


(function TestConstructorCall(){
  var realmIndex = Realm.create();
  var otherTypeError = Realm.eval(realmIndex, "TypeError");
  var A = Realm.eval(realmIndex, '"use strict"; class A {}; A');
  var instance = new A();
  var constructor = instance.constructor;
  var otherTypeError = Realm.eval(realmIndex, 'TypeError');
  if (otherTypeError === TypeError) {
    throw Error('Should not happen!');
  }

  // ES6 9.2.1[[Call]] throws a TypeError in the caller context/Realm when the
  // called function is a classConstructor
  assertThrows(function() { Realm.eval(realmIndex, "A()") }, otherTypeError);
  assertThrows(function() { instance.constructor() }, TypeError);
  assertThrows(function() { A() }, TypeError);

  // ES6 9.3.1 call() first activates the callee context before invoking the
  // method. The TypeError from the constructor is thus thrown in the other
  // Realm.
  assertThrows(function() { Realm.eval(realmIndex, "A.call()") },
      otherTypeError);
  assertThrows(function() { constructor.call() }, otherTypeError);
  assertThrows(function() { A.call() }, otherTypeError);
})();


(function TestConstructorCallOptimized() {
  class A { };

  function invoke_constructor() { A() }
  function call_constructor() { A.call() }
  function apply_constructor() { A.apply() }
  %PrepareFunctionForOptimization(invoke_constructor);
  %PrepareFunctionForOptimization(call_constructor);
  %PrepareFunctionForOptimization(apply_constructor);

  for (var i=0; i<3; i++) {
    assertThrows(invoke_constructor);
    assertThrows(call_constructor);
    assertThrows(apply_constructor);
  }
  // Make sure we still check for class constructors when calling optimized
  // code.
  %OptimizeFunctionOnNextCall(invoke_constructor);
  assertThrows(invoke_constructor);
  %OptimizeFunctionOnNextCall(call_constructor);
  assertThrows(call_constructor);
  %OptimizeFunctionOnNextCall(apply_constructor);
  assertThrows(apply_constructor);
})();


(function TestDefaultConstructor() {
  var calls = 0;
  class Base {
    constructor() {
      calls++;
    }
  }
  class Derived extends Base {}
  var object = new Derived;
  assertEquals(1, calls);

  calls = 0;
  assertThrows(function() { Derived(); }, TypeError);
  assertEquals(0, calls);
})();


(function TestDefaultConstructorArguments() {
  var args, self;
  class Base {
    constructor() {
      self = this;
      args = arguments;
    }
  }
  class Derived extends Base {}

  new Derived;
  assertEquals(0, args.length);

  new Derived(0, 1, 2);
  assertEquals(3, args.length);
  assertTrue(self instanceof Derived);

  var arr = new Array(100);
  var obj = {};
  assertThrows(function() {Derived.apply(obj, arr);}, TypeError);
})();


(function TestDefaultConstructorArguments2() {
  var args;
  class Base {
    constructor(x, y) {
      args = arguments;
    }
  }
  class Derived extends Base {}

  new Derived;
  assertEquals(0, args.length);

  new Derived(1);
  assertEquals(1, args.length);
  assertEquals(1, args[0]);

  new Derived(1, 2, 3);
  assertEquals(3, args.length);
  assertEquals(1, args[0]);
  assertEquals(2, args[1]);
  assertEquals(3, args[2]);
})();
