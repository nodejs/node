// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --preparser-scope-analysis

function f() {
  new class extends Object {
    constructor() {
      eval("super(); super.__f_10();");
    }
  }
}
assertThrows(f, TypeError);

function g() {
  let obj = {
    m() {
      eval("super.foo()");
    }
  }
  obj.m();
}
assertThrows(g, TypeError);

function h() {
  let obj = {
    get m() {
      eval("super.foo()");
    }
  }
  obj.m;
}
assertThrows(h, TypeError);
