// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --verify-heap

for (let i = 0; i < 5; i++) {
  const src = { maxByteLength: 1073741825 };
  const dst = { maxByteLength: 2754, ...src, maxByteLength: i };
  assertEquals(i, dst.maxByteLength);
}
