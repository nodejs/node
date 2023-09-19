// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --throws

(function bar() {
  (function foo(
      x = new class B extends A(eval) { }
    ) {
        eval();
    })();
  eval();
})()
