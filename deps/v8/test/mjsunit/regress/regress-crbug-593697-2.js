// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --stress-inline

"use strict";

var f5 = (function f6(stdlib) {
  "use asm";
  var cos = stdlib.Math.cos;
  function f5() {
    return cos();
  }
  return { f5: f5 };
})(this, {}).f5();
