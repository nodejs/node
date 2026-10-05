// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --homomorphic-ic --turbofan --no-maglev

function createNormalObject() {
  function C() {}
  %CompleteInobjectSlackTracking(new C());
  return new C();
}

// 1. Interceptor object: Turbofan homomorphic access must deopt.
{
  function load(o) {
    return o.a;
  }
  %PrepareFunctionForOptimization(load);
  for (let i = 0; i < 11; i++) {
    let o = (i % 2 === 0) ? createNormalObject()
                          : d8.test.createSpecialObject({});
    o.a = i;
    load(o);
  }
  %OptimizeFunctionOnNextCall(load);
  let sample = createNormalObject();
  sample.a = 1;
  assertEquals(1, load(sample));
  assertOptimized(load);

  let obj = d8.test.createInterceptorObject(() => "intercepted");
  obj.a = 42;
  assertEquals("intercepted", load(obj));
  assertUnoptimized(load);
}

// 2. Access checked object: Turbofan homomorphic access must deopt.
{
  function load(o) {
    return o.a;
  }
  %PrepareFunctionForOptimization(load);
  for (let i = 0; i < 11; i++) {
    let o = (i % 2 === 0) ? createNormalObject()
                          : d8.test.createSpecialObject({});
    o.a = i;
    load(o);
  }
  %OptimizeFunctionOnNextCall(load);
  let sample = createNormalObject();
  sample.a = 1;
  assertEquals(1, load(sample));
  assertOptimized(load);

  let obj = d8.test.createAccessCheckedObject(true);
  obj.a = 42;
  d8.test.setAccessPolicy(obj, false);

  assertThrows(() => load(obj), TypeError);
  assertUnoptimized(load);
}
