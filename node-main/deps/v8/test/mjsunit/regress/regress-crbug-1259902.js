// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy-feedback-allocation
// Flags: --invocation-count-for-turbofan=1

Object.prototype[1] = 153;
Object.freeze(Object.prototype);
class B {
  [1] = 7;
}
let b = new B();
b = new B();
