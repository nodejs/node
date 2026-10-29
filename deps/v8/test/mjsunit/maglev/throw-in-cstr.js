// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --maglev --allow-natives-syntax

class Base {
  constructor() {
  }
}
class Class extends Base {
  constructor() {
    try {
      // Super initialises the "this" pointer.
      super();
      throw new Error();
    } catch (e) {
      // Access to "this" should be valid.
      assertNotNull(this);
    }
  }
};
%PrepareFunctionForOptimization(Class);
new Class();
new Class();
%OptimizeMaglevOnNextCall(Class);
new Class();
