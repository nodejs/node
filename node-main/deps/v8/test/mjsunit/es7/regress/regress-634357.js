// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-slow-asserts

array = new Array({}, {}, {});
Object.defineProperty(array, 1, {
  get: function() {
    array.length = 0;
    array[0] = -2147483648;
  }
});
result = array.includes(new Array());
