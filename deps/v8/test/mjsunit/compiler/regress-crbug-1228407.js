// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --interrupt-budget=100

function foo() {
  return function bar() {
    a.p = 42;
    for (let i = 0; i < 100; i++) this.p();
    this.p = a;
  };
}

var a = foo();
var b = foo();

a.prototype = { p() {} };
b.prototype = { p() {
  this.q = new a();
  for (let i = 0; i < 200; i++) ;
}};

new b();
