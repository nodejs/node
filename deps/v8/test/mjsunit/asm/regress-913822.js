// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function TestNewlineInCPPComment() {
  function Module() {
    "use asm" // Crash by comment!
    function f() {}
    return f
  }
  Module();
})();

(function TestNewlineInCComment() {
  function Module() {
    "use asm" /* Crash by
    comment! */ function f() {}
    return f
  }
  Module();
})();
