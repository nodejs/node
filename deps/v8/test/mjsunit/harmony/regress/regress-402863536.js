// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --always-turbofan

class C1 {
  constructor() {
    return globalThis;
  }
}
class C2 extends C1 {
  field = 'abc';
}
new C2();
new C2();
Object.seal(globalThis);

assertThrows(() => new C2(), TypeError);
