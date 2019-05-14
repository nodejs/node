// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --debug-code

"use strict";
class Base {
}
class Derived extends Base {
  constructor() {
    eval();
  }
}
assertThrows("new Derived()", ReferenceError);
