// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let o1 = { a: 1, b: 0 };
let o2 = { a: 2, b: 0 };
assertTrue(%HaveSameMap(o1, o2));
assertTrue(%HasOwnConstDataProperty(o1, "a"));
assertTrue(%HasOwnConstDataProperty(o1, "b"));

Object.defineProperty(o1, "b", {
  value: 4.2, enumerable: true, configurable: true, writable: true,
});
assertFalse(%HaveSameMap(o1, o2));
assertTrue(%HasOwnConstDataProperty(o1, "a"));
assertFalse(%HasOwnConstDataProperty(o1, "b"));
assertTrue(%HasOwnConstDataProperty(o2, "a"));
assertTrue(%HasOwnConstDataProperty(o2, "b"));

let o3 = { a: "foo", b: 0 };
assertFalse(%HaveSameMap(o2, o3));
assertTrue(%HasOwnConstDataProperty(o3, "a"));
assertFalse(%HasOwnConstDataProperty(o3, "b"));

Object.defineProperty(o2, "a", {
  value:2, enumerable: false, configurable: true, writable: true,
});
assertTrue(%HasOwnConstDataProperty(o1, "a"));
assertFalse(%HasOwnConstDataProperty(o1, "b"));
assertTrue(%HasOwnConstDataProperty(o3, "a"));
assertFalse(%HasOwnConstDataProperty(o3, "b"));

assertFalse(%HasOwnConstDataProperty(o2, "a"));
assertTrue(%HasOwnConstDataProperty(o2, "b"));
