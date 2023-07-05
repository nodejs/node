// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const kArraySize = 1024;

let array = [];
for (let i = 1; i < kArraySize; ++i) {
  array[i] = i + 0.1;
}

assertEquals(array.length, kArraySize);

let executed = false;
compareFn = _ => {
  if (!executed) {
    executed = true;

    array.length = 1; // shrink
    array.length = 0; // replace
    array.length = kArraySize; // restore the original length
  }
}

array.sort(compareFn);
