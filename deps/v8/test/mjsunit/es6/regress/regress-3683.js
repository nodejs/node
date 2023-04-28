// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

// Simplest case
var count = 0;
for (let x = 0; x < 10;) {
  x++;
  count++;
  continue;
}
assertEquals(10, count);

// Labeled
count = 0;
label: for (let x = 0; x < 10;) {
  while (true) {
    x++;
    count++;
    continue label;
  }
}
assertEquals(10, count);

// Simple and labeled
count = 0;
label: for (let x = 0; x < 10;) {
  x++;
  count++;
  continue label;
}
assertEquals(10, count);

// Shadowing loop variable in same scope as continue
count = 0;
for (let x = 0; x < 10;) {
  x++;
  count++;
  {
    let x = "hello";
    continue;
  }
}
assertEquals(10, count);

// Nested let-bound for loops, inner continue
count = 0;
for (let x = 0; x < 10;) {
  x++;
  for (let y = 0; y < 2;) {
    y++;
    count++;
    continue;
  }
}
assertEquals(20, count);

// Nested let-bound for loops, outer continue
count = 0;
for (let x = 0; x < 10;) {
  x++;
  for (let y = 0; y < 2;) {
    y++;
    count++;
  }
  continue;
}
assertEquals(20, count);

// Nested let-bound for loops, labeled continue
count = 0;
outer: for (let x = 0; x < 10;) {
  x++;
  for (let y = 0; y < 2;) {
    y++;
    count++;
    if (y == 2) continue outer;
  }
}
assertEquals(20, count);
