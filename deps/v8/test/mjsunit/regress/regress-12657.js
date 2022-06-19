// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --gc-global --expose-statistics --max-semi-space-size=1

const a = new Array();
for (var i = 0; i < 50000; i++) {
  a[i] = new Object();
}
assertTrue(getV8Statistics().new_space_commited_bytes <= 2 * 1024 * 1024);
