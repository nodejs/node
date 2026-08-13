// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --force-slow-path

const arr = [1];
function foo() {
  arr.length = 0;
  arr[0] = 0;
  arr.shift(1);
  return 123;
}

Object.defineProperty(arr, 0, {
  get: foo
});

assertEquals(false, arr.includes(undefined));
assertEquals(-1, arr.indexOf(undefined));
