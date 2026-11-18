// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --stress-compaction

let keep = { a: 1.1, k: 1, c: 1, d: 1 };
let arr = [];
for (let i = 0; i < 1535; i++) {
  arr.push(`{"a":1.1,"p${i}":1,"c":1,"d":1}`);
}
let json_str = `[${arr.join(',')}]`;

let fns = [];
for (let i = 0; i < 40; i++) {
  fns.push(new Function('x', `
    return {
      a: 1.1,
      b: 1,
      c: { a: 1.1, b: 1, c: { v: ${i} }, d: 1.1 },
      d: x
    };
  `));
}

gc();
JSON.parse(json_str);
gc({ type: 'minor' });

for (let i = 0; i < 1220; i++) {
  new Array(200);
}
for (let f of fns) {
  f();
  f();
}
