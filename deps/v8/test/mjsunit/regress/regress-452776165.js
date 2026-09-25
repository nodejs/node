// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --for-of-optimization

assertThrows(() => {
  let iterProto = Object.getPrototypeOf(new Uint8Array(1)[Symbol.iterator]());
  iterProto.next = 1;
  for (let _ of new Uint8Array(1)) {
  }
}, TypeError);

assertThrows(() => {
  const iter = [1, 2, 3][Symbol.iterator]();
  iter.next = null;
  for (let _ of iter) {
  }
}, TypeError);
