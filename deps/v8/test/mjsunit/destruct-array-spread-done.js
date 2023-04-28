// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

let called = 0
const it = {
  [Symbol.iterator]() {
    return this;
  },
  next() {
    called += 1;
    return {
      value: 42,
      done: true,
    };
  },
};

const [a, b, ...c] = it;

assertEquals(called, 1);
assertEquals(a, undefined);
assertEquals(b, undefined);
assertEquals(c.length, 0);
