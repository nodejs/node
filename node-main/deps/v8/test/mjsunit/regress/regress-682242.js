// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --es-staging

class BaseClass {
  method() {
    return 1;
  }
}
class SubClass extends BaseClass {
  method(...args) {
    return super.method(...args);
  }
}
var a = new SubClass();
a.method();
