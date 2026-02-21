// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --gc-interval=100

let xs = [];
for (let i = 0; i < 205; ++i) {
  xs.push(i);
}
xs.sort((a, b) => {
  xs.shift();
  xs[xs.length] = -246;
  return a - b;
});
