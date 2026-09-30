// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The interceptor is invoked on failed access checks (e.g. cross-origin
// window/location handling).
{
  let obj = d8.test.createAccessCheckedInterceptorObject(false,
    () => "intercepted-on-denied");
  assertEquals("intercepted-on-denied", obj.a);
  d8.test.setAccessPolicy(obj, true);
  obj.a = 42;
  assertEquals(42, obj.a);
}

// Input validation.
assertThrows(() => d8.test.createAccessCheckedInterceptorObject(), Error);
assertThrows(() => d8.test.createAccessCheckedInterceptorObject(true), Error);
assertThrows(() =>
  d8.test.createAccessCheckedInterceptorObject(true, "notfunction"), Error);
assertThrows(() => d8.test.createAccessCheckedInterceptorObject(123, () => {}),
  Error);
