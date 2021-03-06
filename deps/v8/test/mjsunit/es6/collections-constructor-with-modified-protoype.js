// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

function TestSetPrototypeModified(ctor) {
  const originalPrototypeAdd = ctor.prototype.add;
  const k1 = {};
  const k2 = {};
  const entries = [k1, k2];
  let addCount = 0;

  ctor.prototype.add = function(value) {
    addCount++;
    originalPrototypeAdd.call(this, value);
    entries.length = 1;
  };
  const set = new ctor(entries);

  assertEquals(1, addCount);
  assertTrue(set.has(k1));
  assertFalse(set.has(k2));

  ctor.prototype.add = originalPrototypeAdd;
}
%PrepareFunctionForOptimization(TestSetPrototypeModified);
TestSetPrototypeModified(Set);
TestSetPrototypeModified(Set);
TestSetPrototypeModified(Set);
%OptimizeFunctionOnNextCall(TestSetPrototypeModified);
TestSetPrototypeModified(Set);
assertOptimized(TestSetPrototypeModified);
%DeoptimizeFunction(TestSetPrototypeModified);

%PrepareFunctionForOptimization(TestSetPrototypeModified);
TestSetPrototypeModified(WeakSet);
TestSetPrototypeModified(WeakSet);
TestSetPrototypeModified(WeakSet);
%OptimizeFunctionOnNextCall(TestSetPrototypeModified);
TestSetPrototypeModified(WeakSet);
assertOptimized(TestSetPrototypeModified);
%DeoptimizeFunction(TestSetPrototypeModified);

function TestMapPrototypeModified(ctor) {
  const originalPrototypeSet = ctor.prototype.set;
  const k1 = {};
  const k2 = {};
  const entries = [[k1, 1], [k2, 2]];
  let setCount = 0;

  ctor.prototype.set = function(key, value) {
    setCount++;
    originalPrototypeSet.call(this, key, value);
    entries.length = 1;
  };
  const map = new ctor(entries);

  assertEquals(1, setCount);
  assertTrue(map.has(k1));
  assertFalse(map.has(k2));

  ctor.prototype.set = originalPrototypeSet;
}
%PrepareFunctionForOptimization(TestMapPrototypeModified);
TestMapPrototypeModified(Map);
TestMapPrototypeModified(Map);
TestMapPrototypeModified(Map);
%OptimizeFunctionOnNextCall(TestMapPrototypeModified);
TestMapPrototypeModified(Map);
assertOptimized(TestMapPrototypeModified);
%DeoptimizeFunction(TestMapPrototypeModified);

%PrepareFunctionForOptimization(TestMapPrototypeModified);
TestMapPrototypeModified(WeakMap);
TestMapPrototypeModified(WeakMap);
TestMapPrototypeModified(WeakMap);
%OptimizeFunctionOnNextCall(TestMapPrototypeModified);
TestMapPrototypeModified(WeakMap);
assertOptimized(TestMapPrototypeModified);
