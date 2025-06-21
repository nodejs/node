// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-lazy-source-positions

try {
  (function () {
    eval(`
      function assertLocation() {}
      (function foo() {
        var x = 1;
        assertLocation();
        throw new Error();
      })();
    `);
  })();
} catch (e) {
  print(e.stack);
}

try {
  (function () {
    var assertLocation = 2;
    (function () {
      eval(`
        function assertLocation() {}
        (function foo() {
          var x = 1;
          assertLocation();
          throw new Error();
        })();
      `);
    })();
  })();
} catch (e) {
  print(e.stack);
}
