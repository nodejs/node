// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stress-compaction --allow-natives-syntax --dispatch-table-gc-interval=1

function outer() {
  return [
    (a) => a + 1,
    (a) => a + 2,
    (a) => a + 3,
    (a) => a + 4,
  ];
}

outer();
