// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const __f_1 = eval(`(function __f_1() {
    class Derived extends Object {
      constructor() {
        ${"this.a=1;".repeat(0x3fffe-8)}
      }
    }
    return Derived;
})`);
assertThrows(() => new (__f_1())());
