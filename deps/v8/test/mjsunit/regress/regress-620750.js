// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --es-staging

function push_a_lot(arr) {
  for (var i = 0; i < 2e4; i++) {
    arr.push(i);
  }
  return arr;
}

__v_13 = push_a_lot([]);
