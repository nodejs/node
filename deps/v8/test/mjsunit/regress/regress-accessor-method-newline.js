// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-decorators

function createClasses() {
  return [class {
    accessor
    () { return 1; }
    static accessor
    () { return 2; }
  }, class {
    acc\u0065ssor
    () { return 3; }
    static acc\u0065ssor
    () { return 4; }
  }];
}

const [C, D] = createClasses();
assertEquals(1, new C().accessor());
assertEquals(2, C.accessor());
assertEquals(3, new D().accessor());
assertEquals(4, D.accessor());
