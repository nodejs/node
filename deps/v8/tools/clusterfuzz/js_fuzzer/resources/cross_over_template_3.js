// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let a = 0;
let b = 0;

class A {
  foo() {}
}

class B extends A {
  foo() {
    console.log(42);
  }
}
