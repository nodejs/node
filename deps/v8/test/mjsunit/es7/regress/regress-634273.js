// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-slow-asserts

array = new Array(undefined, undefined, undefined);
Object.defineProperty(array, 0, {
  get: function() {
    array.push(undefined, undefined);
  }
});
array[0x80000] = 1;
result = array.includes(new WeakMap());
