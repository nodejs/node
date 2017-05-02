// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm

var foo = (function(stdlib) {
  "use asm";
  var bar = (stdlib[0]);
  function foo() { return bar ("lala"); }
  return foo;
})(this);

try {
  nop(foo);
  foo();
} catch (e) {
}
