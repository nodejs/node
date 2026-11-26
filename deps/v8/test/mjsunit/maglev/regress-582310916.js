// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

globalThis.retPrimitive = () => 0;
const callablePrimitive = %GetCallable("retPrimitive");

globalThis.retObject = () => ({ x: 42 });
const callableObject = %GetCallable("retObject");

class A {}
class B {}

function testForIn(ctor) {
  for (let k in new ctor()) {}
}

%PrepareFunctionForOptimization(testForIn);
testForIn(A);
testForIn(B);
%OptimizeFunctionOnNextCall(testForIn);
assertThrows(() => testForIn(callablePrimitive), TypeError);
testForIn(callableObject);

class Base {
  constructor(ctor) {
    return new ctor();
  }
}

function testConstruct(ctor) {
  return new Base(ctor);
}

%PrepareFunctionForOptimization(Base);
%PrepareFunctionForOptimization(testConstruct);
assertEquals("object", typeof testConstruct(A));
assertEquals("object", typeof testConstruct(B));
%OptimizeFunctionOnNextCall(testConstruct);
assertThrows(() => testConstruct(callablePrimitive), TypeError);
assertEquals(42, testConstruct(callableObject).x);
