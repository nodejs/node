// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const arr = new Uint8Array([1, 2, 3]);

function mapper(x) {
  arr[1] = 182;
  return x + 1;
}

assertArrayEquals([2, 3, 4], Uint16Array.from(arr, mapper));
