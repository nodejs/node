// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-maglev --turbolev  --invocation-count-for-turbofan=100

class B extends Object {
  constructor() {
    try {
      var g = () => super();
      super();
    } catch (e) {
      g();
    }
  }
}
for (let i = 0; i < 10000; i++) new B();
