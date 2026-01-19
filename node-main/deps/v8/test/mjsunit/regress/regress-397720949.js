// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-staging

const ab = new ArrayBuffer(4, { "maxByteLength": 8 });
const f16 = new Float16Array(ab);
const evil = {
  valueOf() {
      ab.resize(0);
      return 1;
  }
};
assertEquals(-1, f16.lastIndexOf(0, evil));
