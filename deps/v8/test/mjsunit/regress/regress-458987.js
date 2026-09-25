// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function () {
  "use asm";

  function g() {}

  runNearStackLimit(g);
})();

function runNearStackLimit(f) {
  function g() { try { g(); } catch(e) { f(); } };
  g();
}
