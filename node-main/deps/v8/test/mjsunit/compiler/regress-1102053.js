// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --interrupt-budget=1024

const v10 =
  {__proto__: [42], a: 1757695453, length: Promise, toString: 1337, d: []};

async function foo(a) {
  a.length;
  for (const k in v10) {
    for (let i = 0; i < k; i++) {}
    for (let i = 0; i < 10; i++) {
      function bar() {}
      while (a < 1) {
        for (const kk of []) await 42;
      }
    }
  }
}

for (let i = 0; i < 2; i++) {
  foo([42]);
}
