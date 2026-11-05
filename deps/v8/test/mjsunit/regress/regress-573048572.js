// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stress-delay-tasks

const i32a = new Int32Array(new SharedArrayBuffer(16));
const res = Atomics.waitAsync(i32a, 0, 0, 1);
assertEquals(true, res.async);
