// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --es-staging

class A extends Function {
  constructor(...args) {
    super(...args);
    this.a = 42;
    this.d = 4.2;
    this.o = 0;
  }
}
var obj = new A("'use strict';");
obj.o = 0.1;
