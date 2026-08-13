// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-immutable-arraybuffer

function store(ta, index, value) {
  "use strict";
  ta[index] = value;
}

// 1. Call with OOB on normal TypedArray to configure IC to ignore OOB.
const normalTa = new Uint8Array(0);
store(normalTa, 0, 1);

// 2. Call with in-bounds on immutable TypedArray. It must throw TypeError.
const ab = new ArrayBuffer(10);
const immutableAb = ab.transferToImmutable();
const immutableTa = new Uint8Array(immutableAb);

assertThrows(() => store(immutableTa, 0, 1), TypeError);
