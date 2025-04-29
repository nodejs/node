// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function TestOverflowSet() {
  const set = new Set();

  for (let i = 0; i < 0xFFFFFFFF; i++) {
    const s = "str_" + i;
    set.add(s, i);
  }
}

assertThrows(TestOverflowSet, RangeError, /Set maximum size exceeded/);
