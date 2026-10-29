// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-lazy-source-positions --stress-lazy-source-positions

(function() {
  ((x = 1) => {
    function foo() {
      x;
    }
    return x;
  })();
})();
