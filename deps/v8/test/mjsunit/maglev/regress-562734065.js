// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --maglev-assert-types

class A {
  constructor(x = 10) {
    this.x = x;
  }
}

class B {
  constructor(x = 20, y = 30) {
    this.x = x + y;
  }
}

function PrimitiveReturningCtor(x = 10) {
  this.x = x;
  return 0;
}

const kCallableResult = function() {
  return 42;
};
function CallableCtor() {
  return kCallableResult;
}

const ProxyCtor = new Proxy(A, {
  construct(target, args, newTarget) {
    return new Proxy(Reflect.construct(target, args, newTarget), {});
  }
});

// 1. Construct
class Base1 {
  constructor(ctor, x) {
    return new ctor(x);
  }
}
function testConstruct(ctor, x) {
  const res = new Base1(ctor, x);
  return typeof res === 'function' ? res() : res.x;
}

%PrepareFunctionForOptimization(Base1);
%PrepareFunctionForOptimization(testConstruct);
assertEquals(15, testConstruct(A, 15));
assertEquals(35, testConstruct(B, 5));
assertEquals(19, testConstruct(PrimitiveReturningCtor, 19));
assertEquals(42, testConstruct(CallableCtor, 0));
assertEquals(99, testConstruct(ProxyCtor, 99));
%OptimizeMaglevOnNextCall(testConstruct);
assertEquals(15, testConstruct(A, 15));
assertEquals(35, testConstruct(B, 5));
assertEquals(19, testConstruct(PrimitiveReturningCtor, 19));
assertEquals(42, testConstruct(CallableCtor, 0));
assertEquals(99, testConstruct(ProxyCtor, 99));
assertMaglevved(testConstruct);

function LazyDeoptInConstruct(v) {
  %DeoptimizeFunction(testConstruct);
  this.x = v;
}
assertEquals(66, testConstruct(LazyDeoptInConstruct, 66));

// 2. ConstructWithSpread
class Base2 {
  constructor(ctor, arr) {
    return new ctor(...arr);
  }
}
function testConstructWithSpread(ctor, arr) {
  const res = new Base2(ctor, arr);
  return typeof res === 'function' ? res() : res.x;
}

%PrepareFunctionForOptimization(Base2);
%PrepareFunctionForOptimization(testConstructWithSpread);
assertEquals(7, testConstructWithSpread(A, [7]));
assertEquals(12, testConstructWithSpread(B, [5, 7]));
assertEquals(23, testConstructWithSpread(PrimitiveReturningCtor, [23]));
assertEquals(42, testConstructWithSpread(CallableCtor, []));
assertEquals(88, testConstructWithSpread(ProxyCtor, [88]));
%OptimizeMaglevOnNextCall(testConstructWithSpread);
assertEquals(7, testConstructWithSpread(A, [7]));
assertEquals(12, testConstructWithSpread(B, [5, 7]));
assertEquals(23, testConstructWithSpread(PrimitiveReturningCtor, [23]));
assertEquals(42, testConstructWithSpread(CallableCtor, []));
assertEquals(88, testConstructWithSpread(ProxyCtor, [88]));
assertMaglevved(testConstructWithSpread);

// 3. ConstructForwardVarargs + derived constructor + lazy deopt
class DerivedBase extends A {
  constructor(ctor, args) {
    super(1);
    return new ctor(...args);
  }
}
function testConstructForwardVarargs(ctor, ...args) {
  const res = new DerivedBase(ctor, args);
  return typeof res === 'function' ? res() : res.x;
}

%PrepareFunctionForOptimization(A);
%PrepareFunctionForOptimization(DerivedBase);
%PrepareFunctionForOptimization(testConstructForwardVarargs);
assertEquals(11, testConstructForwardVarargs(A, 11));
assertEquals(25, testConstructForwardVarargs(B, 10, 15));
assertEquals(31, testConstructForwardVarargs(PrimitiveReturningCtor, 31));
assertEquals(42, testConstructForwardVarargs(CallableCtor));
assertEquals(55, testConstructForwardVarargs(ProxyCtor, 55));
%OptimizeMaglevOnNextCall(testConstructForwardVarargs);
assertEquals(11, testConstructForwardVarargs(A, 11));
assertEquals(25, testConstructForwardVarargs(B, 10, 15));
assertEquals(31, testConstructForwardVarargs(PrimitiveReturningCtor, 31));
assertEquals(42, testConstructForwardVarargs(CallableCtor));
assertEquals(55, testConstructForwardVarargs(ProxyCtor, 55));
assertMaglevved(testConstructForwardVarargs);

// Trigger lazy deopt inside `new ctor(...args)` while `DerivedBase` is on stack.
function LazyDeoptCtor(v) {
  %DeoptimizeFunction(testConstructForwardVarargs);
  this.x = v;
}
assertEquals(77, testConstructForwardVarargs(LazyDeoptCtor, 77));
