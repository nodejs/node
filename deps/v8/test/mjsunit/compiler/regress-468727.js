// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --noanalyze-environment-liveness

function f() {
  var __v_7 = -126 - __v_3;
  var __v_17 = ((__v_15 & __v_14) != 4) | 16;
  if (__v_17) {
    var __v_11 = 1 << __v_7;
  }
  __v_12 >>= __v_3;
}

assertThrows(f);
