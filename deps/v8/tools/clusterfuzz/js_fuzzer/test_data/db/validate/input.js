// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let v1 = 0;

// This code creates some error-free snippets.
class A {}

class B extends A {
  constructor(param) {
    super([]);
  }

  foo() {
    super.valueOf();
  }
}

Math.floor(v1);

// This code creates error-prone snippets
/a/.search(v1);
v1.setUint16(8, 0xabcd)
delete v1;
