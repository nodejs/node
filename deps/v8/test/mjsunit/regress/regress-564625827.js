// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stress-incremental-marking

this.performance.measureMemory();
const kRingSize = 2048;
const ring = new Array(kRingSize);
const sabRing = new Array(kRingSize);
let index = 0;
for (let i = 0; i < 100000; i++) {
  const ta = new Uint8Array();
  const prev = ring[index];
  if (prev !== undefined) {
    prev.buffer;
  }
  ring[index] = ta;

  const gsab = new SharedArrayBuffer(16, {maxByteLength: 64});
  const prevSab = sabRing[index];
  if (prevSab !== undefined) {
    prevSab.grow(32);
  }
  sabRing[index] = gsab;

  index = (index + 1) % kRingSize;
}
