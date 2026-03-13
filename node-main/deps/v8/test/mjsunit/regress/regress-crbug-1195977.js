// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

class Foo extends Float64Array {}

let u32 = new Foo(1000);
u32.__defineSetter__('length', function() {});

class MyArray extends Array {
  static get [Symbol.species]() {
    return function() { return u32; }
  };
}

var w = new MyArray(300);
w.fill(1.1);
w[1] = {
  valueOf: function() {
    w.length = 1;
    gc();
    return 1.1;
  }
};
var c = Array.prototype.concat.call(w);

for (var i = 0; i < 32; i++) {
  print(u32[i]);
}
gc();
