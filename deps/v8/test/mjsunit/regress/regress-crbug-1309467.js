// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let caught = false;

function main() {
  class B {
    m() {
      try {
        return super.nodeType;  // The access site (megamorphic)
      } catch (e) {
        caught = true;
      }
    }
  }
  const node = new d8.dom.Div(); // API obj
  B.prototype.__proto__ = node; // Lookup start obj == API obj
  const b = new B();

  b.x0 = 2;
  b.x1 = 10;
  b.x2 = 3;
  b.x3 = 4;
  for (let i = 0; i < 20000; i++) {
    caught = false;
    b.m();
    assertTrue(caught);
  }
}
main();
