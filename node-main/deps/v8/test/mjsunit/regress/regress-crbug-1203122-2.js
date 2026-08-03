// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function main() {
  class A {}
  A.prototype.prototype = 'a string';
  class C extends A {
    m() {
      super.prototype;
    }
  }
  function f() {}

  // Create handler; receiver is a function.
  C.prototype.m.call(f);
  // Use handler; receiver not a function.
  C.prototype.m.call('not a function');
}

for (let i = 0; i < 100; ++i) {
  main();
}
