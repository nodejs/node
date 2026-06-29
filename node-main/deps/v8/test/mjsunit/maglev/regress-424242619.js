// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation
// Flags: --maglev-poly-calls

const v1 = [-9223372036854775808125284];
class C2 {
  constructor(a4) {
    const v6 ="MIN_VALUE".toLowerCase(...a4, ...a4);
    const t4 = this.constructor;
    new t4(v6);
  }
}
try {
  new C2(v1);
} catch {
}
