// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --omit-default-ctors --sparkplug --maglev
// Flags: --turbofan --no-always-sparkplug

// When new.target is not a JSFunction with a prototype slot, computing the
// instance map runs user code (new.target.prototype), so all tiers must take
// the regular Construct path instead of creating the instance directly. On the
// regular path, the spread in super(...args) is iterated.

function makeIterable() {
  const iterable = {
    count: 0,
    [Symbol.iterator]() {
      ++this.count;
      return [][Symbol.iterator]();
    }
  };
  return iterable;
}

function construct(B, newTarget, expectedProto) {
  const iterable = makeIterable();
  const o = Reflect.construct(B, [iterable], newTarget);
  assertSame(expectedProto, Object.getPrototypeOf(o));
  assertEquals(1, iterable.count);
  assertThrows(() => Reflect.construct(B, [123], newTarget), TypeError);
}

// Each test defines its own classes, since compiled code is shared between
// closures of the same function literal.
function testAllTiers(B, newTarget, expectedProto) {
  %PrepareFunctionForOptimization(B);

  construct(B, newTarget, expectedProto);
  assertTrue(isInterpreted(B));

  %CompileBaseline(B);
  construct(B, newTarget, expectedProto);
  assertTrue(isBaseline(B));

  %OptimizeMaglevOnNextCall(B);
  construct(B, newTarget, expectedProto);
  assertMaglevved(B);

  %OptimizeFunctionOnNextCall(B);
  construct(B, newTarget, expectedProto);
  assertOptimized(B);
}

(function TestProxyNewTarget() {
  class A {}  // Default base ctor.
  class B extends A {
    constructor(args) {
      super(...args);
    }
  }
  const proto = {};
  function F() {}
  F.prototype = proto;
  testAllTiers(B, new Proxy(F, {}), proto);
})();

(function TestBoundFunctionNewTarget() {
  class A {}  // Default base ctor.
  class B extends A {
    constructor(args) {
      super(...args);
    }
  }
  // A bound function has no "prototype" property, so the instance gets
  // %Object.prototype% from the realm.
  testAllTiers(B, function() {}.bind(), Object.prototype);
})();
