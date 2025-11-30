// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-base-64 --stress-scavenger-conservative-object-pinning-random

for (let i = 0; i < 25; i++) {
  for (let j = 0; j < 5; j++) {
    new Float64Array(2418);
    const array = new Uint8Array(i);
    array.toBase64();
  }
}
