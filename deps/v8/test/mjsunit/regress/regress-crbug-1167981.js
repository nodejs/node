// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class A {
  constructor() {
    x => { super[y] = 55; };
    class x extends Object() { constructor() {} };
    new x();
  }
};
assertThrows(() => {new A();});
