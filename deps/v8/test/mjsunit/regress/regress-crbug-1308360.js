// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let badCaseRan = false;

function main(i) {
  class B {
    m() {
      return super.nodeType;  // The access site (megamorphic)
    }
  }
  let node = new d8.dom.Div();  // API object
  node['a' + i] = 1;  // Create a new shape
  if (i < 0x100 - 1) {
    B.prototype.__proto__ = {};
  } else {
    B.prototype.__proto__ = node;  // Lookup start object == API object
  }
  let b = new B();

  b.x0 = 1;
  b.x1 = 2;
  b.x2 = 3;
  b.x3 = 4;
  node.nodeType;  // Create a handler for loading from the API object
  let caught = false;
  try {
    b.m();
  } catch {
    caught = true;
  }
  if (i < 0x100 - 1) {
    assertFalse(caught);
  } else {
    assertTrue(caught);
    badCaseRan = true;
  }
}

for (let i = 0; i < 0x100; i++) {
  main(i);
}
assertTrue(badCaseRan);
