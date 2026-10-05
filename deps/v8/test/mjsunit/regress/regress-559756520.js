// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

{
  const arr = Object.freeze([1]);
  assertThrows(() => %StoreInArrayLiteralIC_Slow(4, arr, 1), TypeError);
}

{
  const arr = [1];
  Object.defineProperty(arr, 'length', { writable: false });
  assertThrows(() => %StoreInArrayLiteralIC_Slow(4, arr, 1), TypeError);
}
