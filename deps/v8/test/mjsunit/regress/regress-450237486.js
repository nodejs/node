// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-immutable-arraybuffer

const buffer = new ArrayBuffer(10);
const immBuf = buffer.transferToImmutable();
const immTA = new Uint8Array(immBuf);

assertThrows(() => {
  immTA.set([1], -1);
}, TypeError);
