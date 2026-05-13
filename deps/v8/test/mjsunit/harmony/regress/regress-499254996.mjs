// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-explicit-resource-management

// Test 1: exported `using` variable (should be immutable)
const resource1 = {
  value: 42,
  [Symbol.dispose]() {}
};

using x = resource1;
export { x };

assertThrows(() => { x = { value: "overwritten", [Symbol.dispose]() {} }; }, TypeError);

// Test 2: exported `await using` variable (should be immutable)
const resource2 = {
  value: 99,
  [Symbol.asyncDispose]() { return Promise.resolve(); }
};

await using y = resource2;
export { y };

assertThrows(() => { y = { value: "overwritten-async", [Symbol.asyncDispose]() { return Promise.resolve(); } }; }, TypeError);

// Test 3: exported `const` variable (control)
export const c = 1;
assertThrows(() => { c = 2; }, TypeError);

// Test 4: non-exported `using` variable (control)
const resource3 = {
  value: 7,
  [Symbol.dispose]() {}
};
using z = resource3;
assertThrows(() => { z = { value: "nope", [Symbol.dispose]() {} }; }, TypeError);
