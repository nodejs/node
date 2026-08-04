// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

(function() {
  var __v_6 = false;
  function f(val, idx) {
    if (idx === 1) {
      gc();
      __v_6 = (val === 0);
    }
  }
  f(.1, 1);
})();
