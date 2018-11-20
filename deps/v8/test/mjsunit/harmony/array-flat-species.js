// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-array-flat

{
  class MyArray extends Array {
    static get [Symbol.species]() {
      return Array;
    }
  }

  const wannabe = new MyArray();
  const flattened = wannabe.flat(Infinity);
  assertEquals(false, flattened instanceof MyArray);
  assertEquals(true, flattened instanceof Array);
}

{
  class MyArray extends Array {
    static get [Symbol.species]() {
      return this;
    }
  }

  const wannabe = new MyArray();
  const flattened = wannabe.flat(Infinity);
  assertEquals(true, flattened instanceof MyArray);
  assertEquals(true, flattened instanceof Array);
}
