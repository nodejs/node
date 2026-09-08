// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// 1. Allowed access.
{
  let allow = true;
  let obj = d8.test.createAccessCheckedObject(() => allow);
  obj.a = 42;
  assertEquals(42, obj.a);

  // 2. Denied access throws TypeError.
  allow = false;
  assertThrows(() => obj.a, TypeError);
}

// 3. Exception in access check callback results in denied access (TypeError).
{
  let obj = d8.test.createAccessCheckedObject(() => {
    throw new Error("access-check-error");
  });
  assertThrows(() => obj.a, TypeError);
}

// 4. Input validation.
assertThrows(() => d8.test.createAccessCheckedObject(), Error);
assertThrows(() => d8.test.createAccessCheckedObject(123), Error);
assertThrows(() => d8.test.createAccessCheckedObject("bad"), Error);
assertThrows(() => d8.test.createAccessCheckedObject({}), Error);
