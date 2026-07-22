// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function TestOverflowMap() {
  const map = new Map();

  for (let i = 0; i < 0xFFFFFFFF; i++) {
    const s = "str_" + i;
    map.set(s, i);
  }
}

assertThrows(TestOverflowMap, RangeError, /Map maximum size exceeded/);
