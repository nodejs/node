// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

class C {
  foo() {
    return 42;
  }
}

class D extends C {
  foo() {
    return (() => eval("super.foo()"))();
  }
}

assertEquals(42, new D().foo());
