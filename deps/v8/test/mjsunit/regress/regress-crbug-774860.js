// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --verify-heap

(function () {
  class F extends Function {}
  let f = new F("'use strict';");
  // Create enough objects to complete slack tracking.
  for (let i = 0; i < 20; i++) {
    new F();
  }
  gc();
})();
