// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --trace-evacuation --minor-ms --stress-compaction

let a = [];
for (let i = 0; i < 10000; i++) {
  a.push(new Array(10));
}
gc();
gc();
