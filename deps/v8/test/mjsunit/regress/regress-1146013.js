// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --no-use-ic --interrupt-budget=1000

try {
} catch (e) {}
class __c_0 extends class {} {
  constructor() {
    let __v_18 = () => {
      for (let __v_19 = 0; __v_19 < 10; __v_19++) {
        this.x;
      }
    };
    super();
    __v_18();
  }
}
for (let __v_21 = 0; __v_21 < 10000; __v_21++) {
  new __c_0();
}
