// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let re = /x/y;
let cnt = 0;
let str = re[Symbol.replace]("x", {
  toString: () => {
    cnt++;
    if (cnt == 2) {
      re.lastIndex = {valueOf: () => {
        re.x = 42;
        return 0;
      }};
    }
    return 'y$';
  }
});
assertEquals("y$", str);
