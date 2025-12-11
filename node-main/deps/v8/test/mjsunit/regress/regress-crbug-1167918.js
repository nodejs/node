// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class A {
}
class B extends A {
  m() {
    let o = {
      m2() {
      }
    };
    () => { super.x; }
  }
}
let b = new B();
b.m();
