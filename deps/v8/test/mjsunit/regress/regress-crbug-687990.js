// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var x = 1;

var foo = (function() {
  "use asm";
  var o = this;
  return function() { o.x = null; }
})();

%OptimizeFunctionOnNextCall(foo);
foo();
