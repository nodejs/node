// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {
  const a = [];
  a[0] = 1;
  return a[0];
}

Object.setPrototypeOf(Array.prototype, new Int8Array());
assertEquals(undefined, foo());
assertEquals(undefined, foo());
