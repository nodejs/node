// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --verify-heap --track-array-buffer-views

// Invalidate the ArrayBufferDetaching protector so TryDetachViews is skipped
// on the subsequent transfer below. This leaves `ab.views` as a live weak ref
// to the Uint8Array even after `ab` is detached.
const ab1 = new ArrayBuffer(64, { maxByteLength: 128 });
ab1.resize(32);

const ab = new ArrayBuffer(64);
new Uint8Array(ab);

const byteLength = { valueOf() { ab.transfer(); return 3; } };
try {
  new DataView(ab, 0, byteLength);
} catch (e) {}
gc();
