// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// 1. Empty options object creates an API object without interceptor or access
// check.
{
  let obj = d8.test.createSpecialObject({});
  obj.a = 42;
  assertEquals(42, obj.a);
}

// 2. Options with interceptor.
{
  let obj = d8.test.createSpecialObject({
    interceptor: () => "intercepted",
  });
  assertEquals("intercepted", obj.a);
}

// 3. Options with accessCheck.
{
  let allow = true;
  let obj = d8.test.createSpecialObject({
    accessCheck: () => allow,
  });
  obj.a = 42;
  assertEquals(42, obj.a);
  allow = false;
  assertThrows(() => obj.a, TypeError);
}

// 4. Options with both interceptor and accessCheck:
// The interceptor is invoked on failed access checks (e.g. cross-origin
// window/location handling).
{
  let allow = false;
  let obj = d8.test.createSpecialObject({
    interceptor: () => "intercepted-on-denied",
    accessCheck: () => allow,
  });
  assertEquals("intercepted-on-denied", obj.a);
  allow = true;
  obj.a = 42;
  assertEquals(42, obj.a);
}

// 5. Input validation.
assertThrows(() => d8.test.createSpecialObject(), Error);
assertThrows(() => d8.test.createSpecialObject(123), Error);
assertThrows(() => d8.test.createSpecialObject("bad"), Error);
assertThrows(() => d8.test.createSpecialObject({ interceptor: 123 }), Error);
assertThrows(() => d8.test.createSpecialObject({ accessCheck: 123 }), Error);
