// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// 1. Basic property interception.
{
  let interceptedProp = null;
  let obj = d8.test.createInterceptorObject((prop) => {
    interceptedProp = prop;
    return 123;
  });
  assertEquals(123, obj.foo);
  assertEquals("foo", interceptedProp);
  assertEquals(123, obj.bar);
  assertEquals("bar", interceptedProp);
}

// 2. Intercepting symbol properties.
{
  let s = Symbol("test");
  let interceptedProp = null;
  let obj = d8.test.createInterceptorObject((prop) => {
    interceptedProp = prop;
    return "symbol-value";
  });
  assertEquals("symbol-value", obj[s]);
  assertEquals(s, interceptedProp);
}

// 3. Exception in interceptor callback propagates.
{
  let obj = d8.test.createInterceptorObject(() => {
    throw new Error("interceptor-error");
  });
  assertThrows(() => obj.foo, Error, "interceptor-error");
}

// 4. Input validation.
assertThrows(() => d8.test.createInterceptorObject(), Error);
assertThrows(() => d8.test.createInterceptorObject(123), Error);
assertThrows(() => d8.test.createInterceptorObject("bad"), Error);
assertThrows(() => d8.test.createInterceptorObject({}), Error);
