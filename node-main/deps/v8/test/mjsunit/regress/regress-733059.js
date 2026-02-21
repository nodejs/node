// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --enable-slow-asserts

a = new Proxy([], {
  defineProperty() {
    b.length = 1; gc();
    return Object.defineProperty.apply(this, arguments);
  }
});

class MyArray extends Array {
  static get[Symbol.species](){
    return function() {
      return a;
    }
  };
}

b = new MyArray(65535);
b[1] = 0.1;
c = Array.prototype.concat.call(b);
gc();
