// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var count = 0;
function keyedSta(a) {
  a[0] = {
    valueOf() {
      count += 1;
      return 42n;
    }
  };
};

array1  = keyedSta(new BigInt64Array(1));
var r = keyedSta(new BigInt64Array());
assertEquals(count, 2);
