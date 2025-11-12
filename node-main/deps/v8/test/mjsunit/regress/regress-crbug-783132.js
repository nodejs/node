// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --verify-heap

function f(o, v) {
  try {
    f(o, v + 1);
  } catch (e) {
  }
  o[v] = 43.35 + v * 5.3;
}

f(Array.prototype, 0);
