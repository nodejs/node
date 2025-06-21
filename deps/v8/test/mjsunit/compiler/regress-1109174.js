// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --interrupt-budget=1024

(function() {
  let n = 0;
  do {
    for (let i = 0; i < 100; i++) {
      function unused() {}
      let a = [i];
      let b = String.fromCodePoint(i, 1).padStart();
      a.reduce(parseInt, b);
    }
    for (let j = 0; j < 1; j++) {}
  } while (++n < 2);
})();
