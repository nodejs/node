// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --homomorphic-ic --no-maglev --no-turbofan

function createNormalObject() {
  function C() {}
  %CompleteInobjectSlackTracking(new C());
  return new C();
}

function load(o) {
  return o.a;
}

%PrepareFunctionForOptimization(load);
// Warm up with a mix of normal JS objects and API objects so that the LoadIC
// enters HOMOMORPHIC state.
for (let i = 0; i < 11; i++) {
  let o = (i % 2 === 0) ? createNormalObject()
                        : d8.test.createSpecialObject({});
  o.a = i;
  assertEquals(i, load(o));
}

let fb = %GetFeedback(load);
if (fb !== undefined) {
  assertEquals(1, fb.length);
  assertContains("HOMOMORPHIC", fb[0][1]);
}

// 1. Interceptor object: homomorphic IC must not bypass the interceptor.
{
  let obj = d8.test.createInterceptorObject(() => 'intercepted');
  obj.a = 42;
  assertEquals('intercepted', load(obj));
}

// 2. Access checked object: homomorphic IC must not bypass access checks.
{
  let obj = d8.test.createAccessCheckedObject(true);
  obj.a = 42;
  assertEquals(42, load(obj));
  d8.test.setAccessPolicy(obj, false);
  assertThrows(() => load(obj), TypeError);
}
