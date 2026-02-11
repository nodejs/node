// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

class C extends Object {
  constructor() {
    for (let i = 0; i < 5; i++) {
      if (!i) {
        super();
      }
    }
  }
}
function opt_me() {
  return Reflect.construct(C, [], WeakMap);
}

opt_me();
opt_me();
opt_me();
opt_me();
opt_me();
let obj = opt_me();

assertThrows(() => obj.set({}, 123), TypeError);
