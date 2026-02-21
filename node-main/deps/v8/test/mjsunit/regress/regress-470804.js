// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --expose-gc

function f() {
  this.foo00 = 0;
  this.foo01 = 0;
  this.foo02 = 0;
  this.foo03 = 0;
  this.foo04 = 0;
  this.foo05 = 0;
  this.foo06 = 0;
  this.foo07 = 0;
  this.foo08 = 0;
  this.foo09 = 0;
  this.foo0a = 0;
  this.foo0b = 0;
  this.foo0c = 0;
  this.foo0d = 0;
  this.foo0e = 0;
  this.foo0f = 0;
  this.foo10 = 0;
  this.foo11 = 0;
  this.foo12 = 0;
  this.foo13 = 0;
  this.foo14 = 0;
  this.foo15 = 0;
  this.foo16 = 0;
  this.foo17 = 0;
  this.foo18 = 0;
  this.foo19 = 0;
  this.foo1a = 0;
  this.foo1b = 0;
  this.foo1c = 0;
  this.foo1d = 0;
  this.foo1e = 0;
  this.foo1f = 0;
  this.d = 1.3;
  gc();
  this.boom = 230;
  this.boom = 1.4;
}

function g() {
  return new f();
}
g();
g();
var o = g();
assertEquals(0, o.foo00);
assertEquals(1.4, o.boom);
