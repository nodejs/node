// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

let rand = n => Math.floor(Math.random() * n);

for (let i = 0; i < 1000; ++i) {
  array = [];
  let len = rand(30);
  for(let i = 0; i < len; ++i) {
    array[i] = [i + 0.1];
  }

  let counter = 0;
  array.sort((a, b) => {
    a = a || [0];
    b = b || [0];

    if (counter++ == rand(30)) {
      array.length = 1;
      gc();
    }
    return a[0] - b[0];
  });
}
