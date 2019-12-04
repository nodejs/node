// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-lazy-source-positions --stress-lazy-source-positions
(function () {
  ((d, e = d) => {
    return d * e;
  })();
})();

try {
  (function () {
    ((d, e = f, f = d) => {
      // Won't get here as the initializers will cause a ReferenceError
    })();
  })();
  assertUnreachable();
} catch (ex) {
  assertInstanceof(ex, ReferenceError);
  // Not using assertThrows because we need to access ex.stack to force
  // collection of source positions.
  print(ex.stack);
}

// Check that spreads in arrow functions work
(function () {
  ((...args) => args)();
})();
