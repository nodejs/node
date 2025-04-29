// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let a = 0;
let b = 0;

for (const a = 0; a < 10000; a++) {
  console.log(a);
  console.log(b);
  b = a;
  b = a;
  b = a;
  a = b;
  a = b;
  a = b;
  f(a);
}

let c = 0;
while(true) {
  if (c++ < 10) break;
  console.log(c);
  a = a + c;
  a = a + c;
  a = a + c;
}

let d = 0;
while(d < 10) {
  if (f()) break;
}

let e = 0;
let f = true;
do console.log(e); while (f);

const g = 10;
for (const i = g; i > 0; i--) {
  console.log(i);
  console.log(g);
  if (strange) {
    i = g;
    i = g;
    i = g;
  }
}

console.log(a);
console.log(g);
