// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function main() {
  class C {
    m() {
      super.prototype;
    }
  }
  // Home object's __proto__ is a function.
  function f() {}
  C.prototype.__proto__ = f;

  let c = new C();

  f.prototype;
  c.m();
}

for (let i = 0; i < 100; ++i) {
  main();
}
