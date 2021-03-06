// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

var proxy = new Proxy([], {
  defineProperty() {
    w.length = 1;  // shorten the array so the backstore pointer is relocated
    gc();          // force gc to move the array's elements backstore
    return Object.defineProperty.apply(this, arguments);
  }
});

class MyArray extends Array {
  // custom constructor which returns a proxy object
  static get[Symbol.species](){
    return function() {
      return proxy;
    }
  };
}

var w = new MyArray(100);
w[1] = 0.1;
w[2] = 0.1;

var result = Array.prototype.concat.call(w);

assertEquals(undefined, result[0]);
assertEquals(0.1, result[1]);

for (var i = 2; i < 200; i++) {
  assertEquals(undefined, result[i]);
}
