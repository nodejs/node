// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function x() {
  let obj = { foo: "bar" };
  obj.toJSON = 42;
  obj.__proto__ = new Proxy([], {});
  assertTrue(%HasFastProperties(obj));
  return obj;
}
assertEquals("[object Object]", toString.call(x()));

function y() {
  let obj = { foo: "bar" };
  Object.defineProperty(obj, "foo", {
    get: function () {}
  });
  obj.toJSON = 42;
  obj.__proto__ = new Proxy([], {});
  assertFalse(%HasFastProperties(obj));
  return obj;
}
assertEquals("[object Object]", toString.call(y()));
