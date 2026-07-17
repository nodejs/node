// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Module(stdlib, imports, buffer) {
  "use asm";
  var a = imports.x | 0;
  function f() {
    return a | 0;
  }
  return { f:f };
}
try {
  Module(this).f();
} catch(e) {
  assertInstanceof(e, TypeError);
  // The following print is needed to cross the API boundary and thereby flush
  // out any leftover scheduled exceptions. Any other API function would do.
  print(e);
}
