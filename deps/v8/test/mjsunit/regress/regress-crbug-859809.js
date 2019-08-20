// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

let xs = [];
const kSize = 200;
for (let i = 0; i < kSize; ++i) {
  xs.push(i);
}

let counter = 0;
xs.sort((a, b) => {
  if (counter++ % 10 == 0) {
    xs.shift();
    gc();
  }

  return a - b;
});
