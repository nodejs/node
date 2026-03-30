// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-turbo-escape --no-turbo-allocation-folding

function f() {
  // Allocating an array that will be initialized with holes.
  let arr = new Array(5);
  // Doing a bunch more allocations in the hope of triggering a GC.
  [arr, 5, 6, 7, [8], [9]];
  [2, 5, 6, 7, [8], [9]];
  [2, 5, 6, 7, [8], [9]];
  [2, 5, 6, 7, [8], [9]];
  [2, 5, 6, 7, [8], [9]];
  [2, 5, 6, 7, [8], [9]];
  [2, 5, 6, 7, [8], [9]];
  [2, 5, 6, 7, [8], [9]];
  [2, 5, 6, 7, [8], [9]];
  // Setting arr[3]. Because previous allocations could trigger GCs, store-store
  // elimination should not remove the initial hole-initialization of arr[3],
  // otherwise the GC could see an undefined field.
  arr[3] = 42;
  return arr;
}

// Running `f` a few times, in the hope that a GC will eventually be scheduled
// inside it.
for (let i = 0; i < 5000; i++) {
  f();
}
