// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --verify-heap --expose-gc

function foo(  )  {
  return [
  0,
  1,
  2,
  3,
  4,
  5,
  6,
  7,
  8,
  9,
  10,
  0x1000000,
  0x40000000,
  12,
  60,
  100,
  1000 * 60 * 60 * 24].map(Math.asin);
}

let b = [];
b.constructor = {};
b.constructor[Symbol.species] = function() {};

let a = [];
for (let i = 0; i < 10; i++) {
  a.push(foo());
  gc();
  gc();
  gc();
}
