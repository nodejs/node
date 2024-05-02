// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

{
  class MyArray extends Array {
    static get [Symbol.species]() {
      return Array;
    }
  }

  const wannabe = new MyArray();
  const result = wannabe.flatMap(x => [x, x]);
  assertEquals(false, result instanceof MyArray);
  assertEquals(true, result instanceof Array);
}

{
  class MyArray extends Array {
    static get [Symbol.species]() {
      return this;
    }
  }

  const wannabe = new MyArray();
  const result = wannabe.flatMap(x => [x, x]);
  assertEquals(true, result instanceof MyArray);
  assertEquals(true, result instanceof Array);
}
