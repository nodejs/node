// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stress-lazy-source-positions

(function f() {
  {
    let force_block_scope;
    eval(`
      function f() {}
      var g = (function g() { f(); })
    `);
  }
})();
