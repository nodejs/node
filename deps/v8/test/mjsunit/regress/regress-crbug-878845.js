// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let arr = [, 0.1];

Array.prototype.lastIndexOf.call(arr, 100, {
  valueOf() {
    arr.length = 0;
  }
});
