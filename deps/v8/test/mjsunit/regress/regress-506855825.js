// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --verify-heap --track-array-buffer-views

const ab = new ArrayBuffer(64);
new Uint8Array(ab);

const evil = { valueOf() { ab.transfer(); return 3; } };
assertThrows(() => new DataView(ab, 0, evil), TypeError);
