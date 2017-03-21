// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function __f_17(__v_9) {
  for (var count = 0; count < 20000; ++count) {
    if (count < 100) {
      __v_9.push(3);
    } else if (count < 2500) {
      __v_9.push(2.5);
    } else {
      __v_9.push(true);
    }
  }
  return __v_9;
}

let a = __f_17([]);
assertEquals(a[0], 3);
assertEquals(a[10], 3);
assertEquals(a[2499], 2.5);
assertEquals(a[10000], true);
